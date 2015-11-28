# Project Description
This project was built using C, and unix libraries such as pthreads and semaphores. The program is designed to read an input file, write to / read from pipes, from child processes. An input file is read from a parent process, and then passed to other child processes using pipes. The program workflow is as follows:
* Process is forked; giving P1, and P2
* P1 reads from input file
* P2 uses semaphore to block until P1 writes to the first pipe
* P1 writes input file data to the first pipe, then unlocks the semaphore
* P2 reads from the first pipe, then locks the semaphore
* P2 is forked; giving P2, and P3
* P3 uses semaphore to block until P2 writes to the second pipe
* P2 writes input file data to the second pipe, then unlocks the semaphore
* P3 reads from the first pipe, then writes to the output file
