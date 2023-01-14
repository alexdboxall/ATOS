
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
*         A                 the file descriptor to close
*         B                 not used
*         C                 not used
*         D                 not used
* Output:
*         0                 on success
*         error code        on failure
*/
int sys_close(size_t args[4]) {
    struct vnode* node = fildesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[0]);
    if (node == NULL) {
        return EBADF;
    }

    return vfs_close(node);
}