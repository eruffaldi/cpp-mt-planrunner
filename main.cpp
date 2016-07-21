#include "staticscheduler.hpp"
#include <mutex>
std::mutex mio;

struct lockio
{
	lockio() : lck(mio) {}
    std::unique_lock<std::mutex> lck;
};

int main(int argc, char const *argv[])
{
	Scheduler s(argv[1]);

	s << 1 << [&] {
		{lockio x; std::cerr << "i1A\n"; }
		usleep(10000);
		{lockio x; std::cerr << "i1B\n"; }
	}	;

	s << 2 << [&] {
		{lockio x; std::cerr << "i2A\n"; }
		usleep(20000);
		{lockio x; std::cerr << "i2B\n"; }
	}	;

	s << 3 <<  [&] {
		{lockio x; std::cerr << "i3A\n"; }
		usleep(30000);
		{lockio x; std::cerr << "i3B\n"; }
	}	;

	s.run();
	return 0;
}