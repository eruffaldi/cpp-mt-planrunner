#pragma once


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

inline int setselfaffinity(int idx)
{
  int core = 1 << idx;
  thread_affinity_policy_data_t policy = { core };
  thread_port_t mach_thread;
   return thread_policy_set(math_thread_self(), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
}

#else

inline int setaffinity(std::thread & t, int idx)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(idx, &cpuset);
	return pthread_setaffinity_np(t.native_handle(),sizeof(cpu_set_t), &cpuset);
}

inline int setselfaffinity(int idx)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(idx, &cpuset);
  return pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t), &cpuset);

}

#endif
