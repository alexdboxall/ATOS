
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <vfs.h>
#include <cpu.h>
#include <thread.h>
#include <vnode.h>
#include <process.h>
#include <filedes.h>
#include <termios.h>
#include <panic.h>

/*
* ...?
*
* Inputs: 
*         A                 the file descriptor to get terminal attributes for
*         B                 the pointer to the termios struct to fill
*         C                 ?
*         D                 ?
* Output:
*         ?
*/
int sys_tcgetattr(size_t args[4]) {
    struct vnode* node = filedesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[0]);
    if (node == NULL) {
        return EBADF;
    }

    if (args[2] != TCSANOW) {
        return EINVAL;
    }
    
    if (!vnode_op_is_tty(node)) {
        return ENOTTY;
    }

    return ENOSYS;
}
