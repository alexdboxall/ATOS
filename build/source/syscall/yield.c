
#include <stddef.h>
#include <thread.h>
#include <kprintf.h>

/*
* Gives up the current thread's timeslice and causes a task switch to occur if possible.
*
* Inputs: 
*         A                 not used
*         B                 not used
*         C                 not used
*         D                 not used
* Output:
*         0                 always returned
*/
int sys_yield(size_t args[4]) {
    (void) args;

    thread_yield();

    return 0;
}