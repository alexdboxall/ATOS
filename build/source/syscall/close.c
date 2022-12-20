
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <panic.h>

/*
* ...?
*
* Inputs: 
*         A                 ...?
*         B                 ...?
*         C                 ...?
*         D                 ...?
* Output:
*         ...?
*/
int sys_close(size_t args[4]) {
    (void) args;

    return ENOSYS;
}