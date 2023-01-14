
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <vfs.h>
#include <cpu.h>
#include <thread.h>
#include <process.h>
#include <filedes.h>
#include <vnode.h>
#include <panic.h>

/*
* Determines whether or not a given file descriptor points to a terminal.
*
* Inputs: 
*         A                 the file descriptor to check
*         B                 not used
*         C                 not used
*         D                 not used
* Output:
*         0                 it is a terminal
*         ENOTTY            not a terminal
*         EBADF             bad file descriptor
*/
int sys_isatty(size_t args[4]) {
    struct vnode* node = fildesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[0]);
    if (node == NULL) {
        return EBADF;
    }

    return vnode_op_is_tty(node) ? 0 : ENOTTY;
}