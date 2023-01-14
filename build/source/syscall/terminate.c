
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <panic.h>

/*
* Immediately terminates the calling thread. If this is the only thread in the process,
* then the process will also be terminated.
*
* Inputs: 
*         A                 not used
*         B                 not used
*         C                 not used
*         D                 not used
* Output:
*         does not return
*/
int sys_terminate(size_t args[4]) {
    (void) args;

    thread_terminate();
    panic("thread_terminate continued execution");
}