
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
*         D                 a pointer to an integer, which the number of bytes written will be put
* Output:
*         0                 on success
*         EINVAL            invalid file descriptor, (et. al.)
*         error code        on failure
*/
int sys_write(size_t args[4]) {
    // TODO: are seek positions stored in vnodes? or do we need to add a field to the filedes table for it?
    size_t offset;
    struct vnode* node = fildesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[2], &offset);
    if (node == NULL) {
        return EBADF;
    }

    struct uio io = uio_construct_read_from_usermode((void*) args[0], args[1], offset);
    int result = vfs_write(node, &io);
    if (result != 0) {
        return result;
    }

    *((int*) args[3]) = args[1] - io.length_remaining;

    return 0;
}