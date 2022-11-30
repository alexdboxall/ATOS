
#include <stddef.h>
#include <thread.h>

/*
* Gives up the current thread's timeslice and causes a task switch to occur if possible.
*
* Inputs: 
*         none
* Output:
*         0                 always returned
*/
int sys_yield(size_t args[4]) {
    (void) args;

    thread_yield();

    return 0;
}