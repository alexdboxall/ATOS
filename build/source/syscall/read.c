
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <vfs.h>
#include <filedes.h>
#include <cpu.h>
#include <process.h>
#include <uio.h>
#include <vnode.h>
#include <kprintf.h>

/*
* Reads data from a file.
*
* Inputs: 
*         A                 the binary data to read
*         B                 the length of the data to read
*         C                 the file descriptor to read to
*         D                 a pointer to an size_t, which the number of bytes read will be put
* Output:
*         0                 on success
*         EINVAL            invalid file descriptor, (et. al.)
*         error code        on failure
*/
int sys_read(size_t args[4]) {
    struct vnode* node = filedesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[2]);
    if (node == NULL) {
        return EBADF;
    }

    struct uio io = uio_construct_write_to_usermode((void*) args[0], args[1], node->seek_position);
    int result = vfs_read(node, &io);
    if (result != 0) {
        return result;
    }

    io = uio_construct_write_to_usermode((size_t*) args[3], sizeof(size_t), 0);
    size_t br = args[1] - io.length_remaining;

    node->seek_position += br;

    return uio_move(&br, &io, sizeof(size_t));
}