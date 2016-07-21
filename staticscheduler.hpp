#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <list>
#include <iostream>
#include <fstream>
#include <sstream>
#include <pthread.h>

#ifdef __APPLE__

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_init.h>
#include <mach/thread_policy.h>

inline int setaffinity(std::thread & t, int idx)
{
  int core = 1 << idx;
  thread_affinity_policy_data_t policy = { core };
  thread_port_t mach_thread;
  if(t.native_handle() == 0)
	  	mach_thread = pthread_mach_thread_np(pthread_self());
  else
	  mach_thread	= pthread_mach_thread_np(t.native_handle());
  //std::cout << "thread id:" << t.get_id() << " native:" << t.native_handle() << " mach:" << mach_thread << std::endl;
  int r = thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
  //\std::cout << "\taffinity set result " << r << std::endl;
  return r;
}

#else

inline int setaffinity(std::thread & t, int idx)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(idx, &cpuset);
	return pthread_setaffinity_np(t.native_handle(),sizeof(cpu_set_t), &cpuset);
}

#endif

class Scheduler;

class semaphore
{
public:

  semaphore(int count_ = 0) : count0(count_),count{count_}
  {}

	void set(int c) { count = count0 = c;}

  void reset() { count = count0; }

  void notify()
  {
    std::unique_lock<std::mutex> lck(mtx);
    ++count;
    cv.notify_one();
  }

  void wait()
  {
    std::unique_lock<std::mutex> lck(mtx);
    while(count == 0)
    {
      cv.wait(lck);
    }

    --count;
  }

private:

  std::mutex mtx;
  std::condition_variable cv;
  int count;
  int count0;
};


class ScheduleStepInit
{
public:
	ScheduleStepInit(Scheduler &s, int i): id(i),sched(s) {}
	int id;
	Scheduler & sched;

	void operator << (std::function<void()> fx); // calls addTask
	void operator << (std::function<void(int from,int to)> fx);  // calls addTask
};

class Scheduler
{
public:
	Scheduler(const char *filename) { std::ifstream inf(filename); load(inf); }

	void load(std::ifstream & inf);

	ScheduleStepInit operator << (int id) { return ScheduleStepInit(*this,id); }

	void addTask(int id, std::function<void()> fx);

	void addTask(int id, std::function<void(int,int)> fx);

	void run();

protected:

	enum class Action  { NUMTHREADS, NUMSEMAPHORES, SEMAPHORE,NUMACTIONS,RUNTASK,RUNTASKPAR,WAIT,NOTIFY,SLEEP}; 

	struct ScheduleItem
	{
		Action option;
		int  tid;
		int  id;
		int  params[3];	
	};

	struct ThreadAction
	{
		Action action;
		int id;
		std::function<void()> fx;
	};

	struct ThreadInfo
	{
		int index;
		std::vector<ThreadAction> actions; // only RUNTASK,RUNTASKPAR,WAIT,NOTIFY,SLEEP
		std::thread thread;
		int allocated = 0;
	};

	void loaditem(const ScheduleItem & item);
	void threadentry(ThreadInfo & ta); // thread entry point for each of them

	std::vector<ThreadInfo> threads;
	std::vector<std::shared_ptr<semaphore> > semaphores;
	std::unordered_map<int,std::pair<int,int> > assigntasks; // setup phase: task -> (thread,action)
	std::unordered_map<int,std::list<ScheduleItem> > assignpartasks; // setup phase: task -> multiple allocations with ranges

};

inline void ScheduleStepInit::operator << (std::function<void()> fx)
{
	sched.addTask(id,fx);
}

inline void ScheduleStepInit::operator << (std::function<void(int from,int to)> fx)
{
	sched.addTask(id,fx);
}

inline void Scheduler::addTask(int id, std::function<void()> fx)
{
	auto p = assigntasks[id];
	threads[p.first].actions[p.second].fx = fx;
}

inline void Scheduler::addTask(int id, std::function<void(int,int)> fx)
{
	for(auto & p : assignpartasks[id])
	{
		int a = p.params[0];
		int b = p.params[1];
		// NOTE: p.id is here the relative index NOT the original taskid
		threads[p.tid].actions[p.id].fx = [fx,a,b] () { fx(a,b); };
	}
}

inline void Scheduler::threadentry(ThreadInfo & ti)
{
	for(auto & p : ti.actions)
	{
		switch(p.action)
		{
			case Action::RUNTASK:
			case Action::RUNTASKPAR:
				p.fx();
				break;
			case Action::WAIT:
				std::cout << "WAIT tid:" << ti.index << " sem:" << p.id << " ptr:" << semaphores[p.id].get() << std::endl;
				semaphores[p.id]->wait();
				break;
			case Action::NOTIFY:
				std::cout << "NOTIFY tid:" << ti.index << " sem:" << p.id <<  " ptr:" << semaphores[p.id].get() << std::endl;
				semaphores[p.id]->notify();
				break;
			case Action::SLEEP:
				usleep(p.id*1000);
				break;
			default:
				break;
		}
	}
}

inline void Scheduler::run()
{
	for(auto & s: semaphores)
		if(s)
			s->reset();
	// skip first

	int idx = 1;
	for(auto it = threads.begin()+1; it != threads.end(); it++, idx++)
	{
		it->index = idx;
		std::thread o(std::bind(&Scheduler::threadentry,std::ref(*this),std::ref(*it)));
		setaffinity(o,idx);
		it->thread.swap(o);
	}
	threads[0].index = 0;
	std::thread x;
	setaffinity(x,0);
	threadentry(threads[0]);

	for(auto it = threads.begin()+1; it != threads.end(); it++)
		it->thread.join();

}

inline void Scheduler::loaditem(const ScheduleItem & p)
{
	std::cout << "load " << (int)p.option << " " << p.tid << " " << p.id << " " << p.params[0] << " " << p.params[1] << std::endl;
	switch(p.option)
	{
		case Action::NUMTHREADS:
			threads.resize(p.params[0]);
			std::cout << "\tNUMTHREADS " << threads.size() << std::endl;
			break;
		case Action::NUMSEMAPHORES:
			semaphores.resize(p.params[0]);
			std::cout << "\tNUMSEMAPHORES " << semaphores.size() << std::endl;
			break;
		case Action::SEMAPHORE:
			if(p.id >= 0 && p.id < semaphores.size())
			{
				semaphores[p.id] = std::make_shared<semaphore>(p.params[0]);
				std::cout << "\tSEMAPHORE " << p.id << " with " << p.params[0] << std::endl;
			}
			else
				std::cout << "\twrong NUMACTIONS\n";
			break;
		case Action::NUMACTIONS:
			if(p.tid >= 0 && p.tid < threads.size())
				threads[p.tid].actions.reserve(p.params[0]);
			else
				std::cout << "\twrong NUMACTIONS\n";
			break;
		case Action::RUNTASK:
			{
				if(p.tid >= 0 && p.tid < threads.size())
				{
					int ia = threads[p.tid].actions.size(); // next action
					assigntasks[p.id] = std::pair<int,int>(p.tid,ia); // link task id to action
					threads[p.tid].actions.push_back({p.option,p.id,std::function<void()>()}); // placeholder
				}
				else
					std::cout << "\twrong RUNTASK\n";
			}
			break;
		case Action::RUNTASKPAR:
			{
				int ia = threads[p.tid].actions.size();
				ScheduleItem pp = p;
				pp.id = ia; // replace task id with index in actions
				assignpartasks[p.id].push_back(pp);
				threads[p.tid].actions.push_back({p.option,p.id,std::function<void()>()}); // placeholder
			}
			break;
		case Action::SLEEP:
			if(p.tid >= 0 && p.tid < threads.size())
			{
				threads[p.tid].actions.push_back({p.option,p.params[0],std::function<void()>()}); // placeholder
			}
			break;
		case Action::WAIT:
		case Action::NOTIFY:	
			if(p.id >= 0 && p.id < semaphores.size() && semaphores[p.id])
				threads[p.tid].actions.push_back({p.option,p.id,std::function<void()>()}); // placeholder
			else
			{
				std::cout << "\tbad semaphore WAIT/NOTIFY sem:" << p.id << " for " << p.tid << " " << semaphores[p.id].get() << std::endl;
			}
			break;
	}
}

inline void Scheduler::load(std::ifstream & inf)
{
	threads.clear();
	assigntasks.clear();
	assignpartasks.clear();
	semaphores.clear();
	std::string line;

	while(getline(inf, line ) )
	{
		if(line.size() == 0)
			continue;
		if(line[0] == '#')
			continue;
		std::istringstream is( line );
      	ScheduleItem it;
      	int q;
      	is >> q >> it.tid >> it.id >> it.params[0] >> it.params[1];
      	it.option = (Action)q;
      	if(is)
      		loaditem(it);
	}
}
