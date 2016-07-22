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
	Scheduler s(argv[1],true);
	return 0;
}