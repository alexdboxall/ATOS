
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
*         0                 on success
*         error code        on failure
*/
int sys_tcgetattr(size_t args[4]) {
    struct vnode* node = filedesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[0]);
    if (node == NULL) {
        return EBADF;
    }

    struct termios* tty_termios = node->dev->termios;
    if (tty_termios == NULL) {
        return ENOTTY;
    }

    struct uio io = uio_construct_write_to_usermode((void*) args[1], sizeof(struct termios), 0);    
    return uio_move(tty_termios, &io, sizeof(struct termios));
}
