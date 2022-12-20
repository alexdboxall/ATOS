#include <filedes.h>
#include <heap.h>
#include <vnode.h>
#include <string.h>
#include <spinlock.h>

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
    table->entries_allocated = 64;

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
        struct vnode** new_o_table = malloc(sizeof(size_t) * table->entries_allocated * 2);

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