
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <vfs.h>
#include <cpu.h>
#include <thread.h>
#include <process.h>
#include <filedes.h>
#include <panic.h>

/*
* ...?
*
* Inputs: 
*         A                 the file descriptor to get terminal attributes for
*         B                 the pointer to the termios struct to read attributes from
*         C                 the optional actions
*         D                 ?
* Output:
*         ?
*/
int sys_tcsetattr(size_t args[4]) {
    struct vnode* node = filedesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[0]);
    if (node == NULL) {
        return EBADF;
    }

    if (!vnode_op_is_tty(node)) {
        return ENOTTY;
    }

    return ENOSYS;
}
