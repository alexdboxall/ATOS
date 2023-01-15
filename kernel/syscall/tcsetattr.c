
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <vfs.h>
#include <cpu.h>
#include <thread.h>
#include <vnode.h>
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
*         0                 on success
*         error code        on failure
*/
int sys_tcsetattr(size_t args[4]) {
    struct vnode* node = filedesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[0]);
    if (node == NULL) {
        return EBADF;
    }

    if (args[2] != TCSANOW) {
        return EINVAL;
    }

    struct termios* tty_termios = node->dev->termios;
    if (tty_termios == NULL) {
        return ENOTTY;
    }

    struct termios user_supplied_termios;

    struct uio io = uio_construct_read_from_usermode((void*) args[1], sizeof(struct termios), 0);    
    int result = uio_move(&user_supplied_termios, &io, sizeof(struct termios));
    if (result != 0) {
        return result;
    }

    tty_termios->c_lflag = user_supplied_termios.c_lflag & (ECHO | ICANON);

    return 0;
}
