#pragma once
#include <thread>
#include <functional>
#include <vector>
#include <mutex>
#include <queue>
#include <future>
#include <memory>
#include "setaffine.hpp"

// single writer-single reader
template <class T>
class queueMTsingle
{
public:
	void push(const T & p)
	{
	    std::unique_lock<std::mutex> lck(mtx);
	    q.push(p);		
	    cv.notify_one();
	}

	void pop(T & p)
	{
	    std::unique_lock<std::mutex> lck(mtx);
	    while(q.empty())
	    {
			cv.wait(lck);
	    }
	    p = q.front();
	    q.pop();
	}
	std::mutex mtx;
	std::condition_variable cv;
	std::queue<T> q;
};

class WorkerPool
{
public:
	using fx_t = void();
	using result_t = void;

	WorkerPool(int n = 0);
	~WorkerPool() { join(); }

	void waitallready();
	void join();
	std::future<result_t> call(int i, std::function<fx_t> fx);
	void spawn(int i, std::function<fx_t> fx);

private:
	static void ready() {};
	void threadentry(int i);
	int nthreads;
	std::vector<queueMTsingle<std::function<fx_t> > > queues;
	std::vector<std::thread> threads;
	std::vector<int> terminated;
};

inline WorkerPool::WorkerPool(int n) : nthreads(n == 0 ? std::thread::hardware_concurrency() : n), threads(nthreads),queues(nthreads),terminated(nthreads)
{
	for(int i = 0; i < nthreads; i++)
	{
		terminated[i] = 0;
		std::thread o(std::bind(&WorkerPool::threadentry,std::ref(*this),i));
		setaffinity(o,i);
		threads[i].swap(o);
	}
}

inline std::future<WorkerPool::result_t> WorkerPool::call(int i, std::function<fx_t> fx)
{
	// cannot use packaged_task due to function
	std::shared_ptr<std::promise<result_t> > p = std::make_shared<std::promise<result_t> > ();
	auto r = p->get_future();
	queues[i].push([p,fx]() { fx(); p->set_value();  }); // in C++14 we have move and named lambda to avoid the shared_ptr
	return r;
}

inline void WorkerPool::spawn(int i, std::function<fx_t> fx)
{
	queues[i].push(fx);
}


inline void WorkerPool::threadentry(int i)
{
	while(!terminated[i])
	{
		std::function<fx_t> p;
		queues[i].pop(p);
		p();
	}
}

inline void WorkerPool::join()
{
	for(int i = 0; i < threads.size(); i++)
		spawn(i,[this,i] { this->terminated[i] = 1; });
	// then wait all
	for(auto & p: threads)
		p.join();
}

inline void WorkerPool::waitallready()
{
	std::vector<std::future<result_t> > results(threads.size());
	for(int i = 0; i < threads.size(); i++)
		results[i] = call(i,std::function<fx_t>(&WorkerPool::ready));
	// then wait all
	for(auto & p: results)
		p.wait();
}