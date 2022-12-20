
#include <stddef.h>
#include <errno.h>
#include <filedes.h>
#include <vfs.h>
#include <thread.h>
#include <process.h>
#include <cpu.h>
#include <kprintf.h>
#include <uio.h>

/*
* Opens a file from a given filepath.
*
* Inputs: 
*         A                 a pointer to the string represeting the filepath
*         B                 the file open flags
*         C                 the file open mode (file creation only)
*         D                 a pointer to an integer, which will be filled with the file descriptor if the operation succeeds
* Output:
*         0                 on success, and the file descriptor is put in the integer pointed to by D
*         error code        on failure
*/
int sys_open(size_t args[4]) {
    char path[512];
    
    int result = uio_move_string_from_usermode(path, (char*) args[0], 511);
    if (result != 0) {
        return result;
    }

    struct vnode* node;
    result = vfs_open(path, args[1], args[2], &node);
    
    if (result != 0) {
        return result;
    }

    int filedes = filedesc_table_register_vnode(current_cpu->current_thread->process->fdtable, node);
    
    struct uio uio = uio_construct_write_to_usermode((void*) args[3], sizeof(int), 0);
    return uio_move(&filedes, &uio, sizeof(int));
}