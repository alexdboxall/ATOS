#include <filedes.h>
#include <heap.h>
#include <vnode.h>
#include <string.h>
#include <spinlock.h>
#include <errno.h>

// TODO: more entries and offsets, and add a flags column (for things like O_NOBLOCK)

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
* There is but one, called O_CLOEXEC.
*/
struct filedes_table {
    int next_fd;
    int entries_allocated;
    struct spinlock lock;
    struct vnode** entries;
    size_t* offsets;
};

struct filedes_table* filedes_table_create(void) {
    struct filedes_table* table = malloc(sizeof(struct filedes_table));

    spinlock_init(&table->lock, "filedes table lock");

    table->next_fd = 0;
    table->entries_allocated = 16;

    table->entries = malloc(sizeof(struct vnode*) * table->entries_allocated);
    table->offsets = malloc(sizeof(size_t) * table->entries_allocated);

    return table;
}

struct filedes_table* filedes_table_copy(struct filedes_table* original) {
    struct filedes_table* new_table = malloc(sizeof(struct filedes_table));

    spinlock_acquire(&original->lock);

    new_table->next_fd = original->next_fd;
    new_table->entries_allocated = original->entries_allocated;

    new_table->entries = malloc(sizeof(struct vnode*) * original->entries_allocated);
    new_table->offsets = malloc(sizeof(size_t) * original->entries_allocated);
    memcpy(new_table->entries, original->entries, original->entries_allocated * sizeof(struct vnode*));
    memcpy(new_table->offsets, original->offsets, original->entries_allocated * sizeof(size_t));

    spinlock_release(&original->lock);
    
    return new_table;
}

int filedesc_seek(struct filedes_table* table, int fd, size_t offset) {
    spinlock_acquire(&table->lock);
    if (fd >= table->next_fd) {
        return EINVAL;
    }
    table->offsets[fd] = offset;
    spinlock_release(&table->lock);

    return 0;
}

struct vnode* fildesc_convert_to_vnode(struct filedes_table* table, int fd, size_t* offset_out) {
    spinlock_acquire(&table->lock);
    if (fd >= table->next_fd) {
        return NULL;
    }
    struct vnode* result = table->entries[fd];
    *offset_out = table->offsets[fd];
    spinlock_release(&table->lock);

    return result;
}

int filedesc_table_register_vnode(struct filedes_table* table, struct vnode* node) {
    spinlock_acquire(&table->lock);

    int fd = table->next_fd++;

    table->entries[fd] = node;
    table->offsets[fd] = 0;

    /* We haven't written realloc yet! (But even if we did, we still would have needed to
    * keep track of the allocated size (so we can double it)).
    */

    if (table->next_fd >= table->entries_allocated) {
        struct vnode** new_e_table = malloc(sizeof(struct vnode*) * table->entries_allocated * 2);
        size_t* new_o_table = malloc(sizeof(size_t) * table->entries_allocated * 2);

        memcpy(new_e_table, table->entries, sizeof(struct vnode*) * table->entries_allocated);
        memcpy(new_o_table, table->offsets, sizeof(size_t) * table->entries_allocated);

        free(table->entries);
        free(table->offsets);
        
        table->entries = new_e_table;
        table->offsets = new_o_table;
        table->entries_allocated *= 2;
    }

    spinlock_release(&table->lock);

    return fd;
}