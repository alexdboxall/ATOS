#include <filedes.h>
#include <heap.h>
#include <vnode.h>
#include <string.h>
#include <spinlock.h>
#include <errno.h>
#include <fcntl.h>
#include <vfs.h>

/*
* vfs/filedesc.c - File Descriptors
*
* Implements file decscriptors and file descriptor tables, which allow for usermode programs 
* to interfact with the underlying virtual filesystem.
*
* Each process contains a file descriptor table, which store all of the file descriptors in
* use by a program. A file descriptor is an integer index into this table. These file descriptors
* are used to refer to files by the system call interface.
*/

struct filedes_entry {
    /*
    * Set to NULL if this entry isn't in use.
    */
    struct vnode* vnode;

    /*
    * The only flag that can live here is FD_CLOEXEC. All other flags live on the filesytem
    * level. This is because FD_CLOEXEC is a property of the file descriptor, not the underlying
    * file itself. (This is important in how dup() works.)
    */
    int flags;
};

/*
* The table of all of the file descriptors in use by a process.
*/
struct filedes_table {
    struct spinlock lock;
    struct filedes_entry entries[MAX_FD_PER_PROCESS];
};

/*
* Creates a new file descriptor table for a process.
*/
struct filedes_table* filedes_table_create(void) {
    struct filedes_table* table = malloc(sizeof(struct filedes_table));

    spinlock_init(&table->lock, "filedes table lock");

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        table->entries[i].vnode = NULL;
    }

    return table;
}

/*
* Copies a file descriptor table. In the new table, all of the same file descriptors will point
* to the same underlying files.
*/
struct filedes_table* filedes_table_copy(struct filedes_table* original) {
    struct filedes_table* new_table = malloc(sizeof(struct filedes_table));

    spinlock_acquire(&original->lock);
    memcpy(new_table->entries, original->entries, sizeof(struct filedes_entry) * MAX_FD_PER_PROCESS);
    spinlock_release(&original->lock);
    
    return new_table;
}

/*
* Given a file descriptor, return the underlying virtual filesystem node.
*/
struct vnode* filedesc_convert_to_vnode(struct filedes_table* table, int fd) {
    spinlock_acquire(&table->lock);
    if (fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return NULL;
    }
    struct vnode* result = table->entries[fd].vnode;
    spinlock_release(&table->lock);

    return result;
}

/*
* Registers a file into the filedescriptor table. Returns the file descriptor that was assigned
* to the file, or -1 on fail (i.e. EMFILE).
*/
int filedesc_table_register_vnode(struct filedes_table* table, struct vnode* node) {
    spinlock_acquire(&table->lock);

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        if (table->entries[i].vnode == NULL) {
            table->entries[i].vnode = node;
            table->entries[i].flags = 0;
            spinlock_release(&table->lock);
            return i;
        }
    }

    spinlock_release(&table->lock);

    return -1;
}

/*
* Removes a file from the file descriptor table. Returns 0 on success.
*/
int filedesc_table_deregister_vnode(struct filedes_table* table, struct vnode* node) {
    spinlock_acquire(&table->lock);

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        if (table->entries[i].vnode == node) {
            table->entries[i].vnode = NULL;
            spinlock_release(&table->lock);
            return 0;
        }
    }

    spinlock_release(&table->lock);

    return EINVAL;
}

/*
* Gets called when exec() is called. Checks all of the files in the table for the FD_CLOEXEC flag,
* and closes the ones that have the flag set.
*/
int filedes_handle_exec(struct filedes_table* table) {
    spinlock_acquire(&table->lock);

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        if (table->entries[i].vnode != NULL) {
            if (table->entries[i].flags & FD_CLOEXEC) {
                vfs_close(table->entries[i].vnode);
                table->entries[i].vnode = NULL;
            }
        }
    }

    spinlock_release(&table->lock);

    return 0;
}

/*
* Duplicates a file descriptor. Both file descriptors will point to the same underlying file.
* Therefore, the new file descriptor will have the same seek position and flags as the old one,
* EXCEPT for the FD_CLOEXEC flag.
*
* This implementes dup().
*/
int filedesc_table_dup(struct filedes_table* table, int oldfd) {
    spinlock_acquire(&table->lock);

    struct vnode* original_vnode = filedesc_convert_to_vnode(table, oldfd);
    if (original_vnode == NULL) {
        spinlock_release(&table->lock);
        return -EBADF;
    }

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        if (table->entries[i].vnode == NULL) {
            table->entries[i].vnode = original_vnode;
            table->entries[i].flags = 0;
            spinlock_release(&table->lock);
            return i;
        }
    }

    spinlock_release(&table->lock);
    return -EMFILE;
}

/*
* Implements dup2().
*/
int filedesc_table_dup2(struct filedes_table* table, int oldfd, int newfd) {
    spinlock_acquire(&table->lock);

    struct vnode* original_vnode = filedesc_convert_to_vnode(table, oldfd);

    /*
    * "If oldfd is not a valid file descriptor, then the call fails,
    * and newfd is not closed."
    */
    if (original_vnode == NULL) {
        spinlock_release(&table->lock);
        return -EBADF;
    }

    /*
    * "If oldfd is a valid file descriptor, and newfd has the same
    * value as oldfd, then dup2() does nothing, and returns newfd."
    */
    if (oldfd == newfd) {
        return newfd;
    }

    struct vnode* current_vnode = filedesc_convert_to_vnode(table, oldfd);
    if (current_vnode != NULL) {
        /*
        * "If the file descriptor newfd was previously open, it is closed
        * before being reused; the close is performed silently (i.e., any
        * errors during the close are not reported by dup2())."
        */
        vfs_close(current_vnode);
    }

    table->entries[newfd].vnode = original_vnode;
    table->entries[newfd].flags = 0;
    
    spinlock_release(&table->lock);

    return newfd;
}

/*
* Implements dup3().
*/
int filedesc_table_dup3(struct filedes_table* table, int oldfd, int newfd, int flags) {
    /*
    * "If oldfd equals newfd, then dup3() fails with the error EINVAL."
    */
    if (oldfd == newfd) {
        return -EINVAL;
    }

    /*
    * Ensure that only valid flags are passed in.
    */
    if (flags & ~O_CLOEXEC) {
        return -EINVAL;
    }

    int result = filedesc_table_dup2(table, oldfd, newfd);
    if (result < 0) {
        return result;
    }

    if (flags & O_CLOEXEC) {
        table->entries[newfd].flags |= FD_CLOEXEC;
    }

    return newfd;
}