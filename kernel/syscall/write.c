
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
#include <fcntl.h>

/*
* Writes data to a file.
*
* Inputs: 
*         A                 the binary data to write
*         B                 the length of the data to write
*         C                 the file descriptor to write to
*         D                 a pointer to an size_t, which the number of bytes written will be put
* Output:
*         0                 on success
*         EINVAL            invalid file descriptor, (et. al.)
*         error code        on failure
*/
int sys_write(size_t args[4]) {
    struct vnode* node = fildesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[2]);   
    if (node == NULL) {
        return EBADF;
    }

    if (node->flags & O_APPEND) {
        struct stat st;
        int result = vnode_op_stat(node, &st);
        if (result != 0) {
            return result;
        }

        node->seek_position = st.st_size;
    }

    struct uio io = uio_construct_read_from_usermode((void*) args[0], args[1], node->seek_position);
    
    int result = vfs_write(node, &io);
    if (result != 0) {
        return result;
    }

    io = uio_construct_write_to_usermode((size_t*) args[3], sizeof(size_t), 0);
    size_t br = args[1] - io.length_remaining;

    node->seek_position += br;

    return uio_move(&br, &io, sizeof(size_t));
}