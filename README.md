# cpp-mt-planrunner
C++11 runner based on static plan


#


This is an example project for a static scheduler that is aimed at taking possibly parallelizable DAG-based code and allocating over a fixed set of system threads precedently organized. The static schedule is provided by an external tool that provide also indications for synchronization.

For limiting the code changing for supporting the use of this system we take advantage of context capture of C++11 lambda functions: this approach will allow a code generator to easily switch between OpenMP and the proposed scheme.

Scheduler sched(inputfile); // input file contains the schedule

// add tasks to the schedule as << identifier, FX
// the FX can be a single shot function () or a partition range function that will receive the start/end points

sched << 1, [&] () { ..... };  
sched << 10, [&] () { ..... };  
sched << 2, [&] () { ..... };  
sched << 3, [&] () { ..... };  
sched << 3, [&] (int first, int last) { ..... };   // this

sched.run();

The implementation works as follows:

- the loaded schedule (text or JSON) specifies one list of semaphore with their (id,count), one list of operation (tasks, block, signal) per system thread. During the << phase each task will assigned to the appropriate thread in the correct slot, while paralell ranges will be multiply associated. At run the threads will be run and each will either run a task from the list, signal from one  of the semaphores, block on one of the semaphores. Then they will terminate.
- to avoid additional parsing (JSON) we can express the following simple textual encoding:

op,tid,param1,param2,param3

op = NUMTHREADS    param1= number of threads
op = NUMSEMAPHORES param1=number of semaphores
op = SEMAPHORE     id=#semaphore param1=count
op = NUMACTIONS    tid=#thread param1=count
op = RUNTASK tid=#thread id=task
op = RUNTASKPART tid=#thread id=task param1=from param2=to
op = WAIT tid=#thread id=semaphoreid
op = SIGNAL tid=#thread id=semaphoreid