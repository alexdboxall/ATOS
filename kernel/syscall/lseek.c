
/*
       off_t lseek(int fd, off_t offset, int whence);
       */


#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <panic.h>
#include <cpu.h>
#include <uio.h>
#include <filedes.h>
#include <process.h>
#include <unistd.h>
#include <sys/types.h>
#include <vnode.h>

/*
* ...?
*
* Inputs: 
*         A                 the file descriptor
*         B                 a pointer to an off_t (the offset), the resulting offset is written back here
*         C                 either SEEK_SET, SEEK_CUR or SEEK_END
*         D                 not used
* Output:
*         0                 on success
*         error code        on failure
*/
int sys_lseek(size_t args[4]) {
    struct uio io = uio_construct_read_from_usermode((void*) args[1], sizeof(off_t), 0);
    off_t offset;
    int result = uio_move(&offset, &io, sizeof(off_t));

    if (result != 0) {
        return result;
    }
    
    struct vnode* node = fildesc_convert_to_vnode(current_cpu->current_thread->process->fdtable, args[0]);
    if (node == NULL) {
        return EINVAL;
    }

    size_t current = node->seek_position;
    
    if (args[2] == SEEK_CUR) {
        offset += current;

    } else if (args[2] == SEEK_END) {
        struct stat st;
        result = vnode_op_stat(node, &st);
        if (result != 0) {
            return result;
        }
        offset += st.st_size;

    } else if (args[2] != SEEK_SET) {
        return EINVAL;
    }

    node->seek_position = offset;

    io = uio_construct_write_to_usermode((void*) args[1], sizeof(off_t), 0);
    return uio_move(&offset, &io, sizeof(off_t));
}