
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <vfs.h>
#include <filedes.h>
#include <cpu.h>
#include <process.h>
#include <uio.h>
#include <kprintf.h>

/*
* Writes data to a file.
*
* Inputs: 
*         A                 the binary data to write
*         B                 the length of the data to write
*         C                 the file descriptor to write to
*         D                 the offset within the file to write
* Output:
*         0                 on success
*         EINVAL            invalid file descriptor, (et. al.)
*         error code        on failure
*/
int sys_write(size_t args[4]) {
    struct vnode* node = fildesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[2]);
    if (node == NULL) {
        return EINVAL;
    }
    struct uio io = uio_construct_read_from_usermode((void*) args[0], args[1], 0);
    return vfs_write(node, &io);
}