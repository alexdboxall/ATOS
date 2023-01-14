#include <filedes.h>
#include <heap.h>
#include <vnode.h>
#include <string.h>
#include <spinlock.h>
#include <errno.h>
#include <fcntl.h>
#include <vfs.h>

/*
* FYI: from here: https://www.gnu.org/software/libc/manual/html_node/Duplicating-Descriptors.html
*
* There are "file status flags", "file descriptor flags" and the seek position.
*
* When you dup(), the "file status flags" *AND* the seek position and *SHARED*.
* That means the "file status flags" and seek position should be at a lower level than the FD.
* (i.e. so both FDs can point to the same lower level info)
* (yes, that means when you dup(), fseek and ftell act on *both*)
* (i.e. all of this should be on the vnode level)
*
* The "file descriptor flags" are *NOT* shared. That means they should kept with the FD.
* 
* The "file status flags" are:
*      access flags (set on open, cannot be changed later): O_RDONLY, O_WRONLY, O_RDWR
*      open time flags (used by open, but not kept around): O_CREAT, O_EXCL, O_TRUNC, etc..
*           -> O_NONBLOCK is also one, but it is only one of its TWO meanings
*      I/O operating modes (set on open, can be changed later): O_APPEND, O_NONBLOCK (the other meaning)
* i.e. these are shared by a dup()
*
* The "file descriptor flags" are unique to every FD.
* There is but one, called FD_CLOEXEC.
*/

struct filedes_entry {
    /*
    * Set to NULL if this entry isn't in use.
    */
    struct vnode* vnode;

    /*
    * The only flag that can live here is FD_CLOEXEC.
    */
    int flags;
};

struct filedes_table {
    struct spinlock lock;
    struct filedes_entry entries[MAX_FD_PER_PROCESS];
};

struct filedes_table* filedes_table_create(void) {
    struct filedes_table* table = malloc(sizeof(struct filedes_table));

    spinlock_init(&table->lock, "filedes table lock");

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        table->entries[i].vnode = NULL;
    }

    return table;
}

struct filedes_table* filedes_table_copy(struct filedes_table* original) {
    struct filedes_table* new_table = malloc(sizeof(struct filedes_table));

    spinlock_acquire(&original->lock);
    memcpy(new_table->entries, original->entries, sizeof(struct filedes_entry) * MAX_FD_PER_PROCESS);
    spinlock_release(&original->lock);
    
    return new_table;
}

struct vnode* fildesc_convert_to_vnode(struct filedes_table* table, int fd) {
    spinlock_acquire(&table->lock);
    if (fd < 0 || fd >= MAX_FD_PER_PROCESS) {
        return NULL;
    }
    struct vnode* result = table->entries[fd].vnode;
    spinlock_release(&table->lock);

    return result;
}

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

int filedesc_table_dup2(struct filedes_table* table, int oldfd, int newfd) {
    spinlock_acquire(&table->lock);

    struct vnode* original_vnode = fildesc_convert_to_vnode(table, oldfd);

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

    struct vnode* current_vnode = fildesc_convert_to_vnode(table, oldfd);
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

int filedesc_table_dup(struct filedes_table* table, int oldfd) {
    spinlock_acquire(&table->lock);

    struct vnode* original_vnode = fildesc_convert_to_vnode(table, oldfd);
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
    return -EINVAL;
}

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