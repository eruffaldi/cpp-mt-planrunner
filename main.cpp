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

	int q = 0;

	s << 1 << [&] {
		{lockio x; std::cerr << "i1A " << q << "\n"; }
		q = 1;
		usleep(10000);
		{lockio x; std::cerr << "i1B " << q << "\n"; }
	}	;

	s << 2 << [&] {
		{lockio x; std::cerr << "i2A " << q << "\n"; }
		q = 2;
		usleep(20000);
		{lockio x; std::cerr << "i2B " << q << "\n"; }
	}	;

	s << 3 <<  [&] {
		{lockio x; std::cerr << "i3A " << q << "\n"; }
		q = 3;
		usleep(30000);
		{lockio x; std::cerr << "i3B " << q << "\n"; }
	}	;

	s.run();
	return 0;
}