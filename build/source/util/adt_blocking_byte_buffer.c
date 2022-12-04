
#include <adt.h>
#include <panic.h>
#include <assert.h>
#include <heap.h>
#include <synch.h>
#include <spinlock.h>
#include <kprintf.h>

/*
* util/adt_character_buffer.c - Blocking Character Buffer
*
* Allows characters to be pushed to a buffer, and then popped from the buffer when a character
* becomes available. Useful for the console, and possibly also pipes.
*
*/

struct adt_blocking_byte_buffer {
    uint8_t* buffer;
    int total_size;
    int used_size;
    int start_pos;
    int end_pos;
    struct semaphore* sem;
    struct spinlock lock;
};

struct adt_blocking_byte_buffer* adt_blocking_byte_buffer_create(int size) {
    struct adt_blocking_byte_buffer* buffer = malloc(sizeof(struct adt_blocking_byte_buffer));

    buffer->buffer = malloc(size);
    buffer->total_size = size;
    buffer->used_size = 0;
    buffer->start_pos = 0;
    buffer->end_pos = 0;
    buffer->sem = semaphore_create(size);
    spinlock_init(&buffer->lock, "blocking byte buffer lock");
    semaphore_set_count(buffer->sem, size);

    return buffer;
}

void adt_blocking_byte_buffer_destroy(struct adt_blocking_byte_buffer* buffer) {
    free(buffer->buffer);
    free(buffer);
}

void adt_blocking_byte_buffer_add(struct adt_blocking_byte_buffer* buffer, uint8_t c) {
    spinlock_acquire(&buffer->lock);

    if (buffer->used_size == buffer->total_size) {
        panic("TODO: adt_blocking_byte_buffer_add: buffer is full");
        spinlock_release(&buffer->lock);
    }

    buffer->buffer[buffer->end_pos] = c;
    buffer->end_pos = (buffer->end_pos + 1) % buffer->total_size;
    buffer->used_size++;
    spinlock_release(&buffer->lock);

    semaphore_release(buffer->sem);
}

/*
* Blocks until there is something to read.
*/
uint8_t adt_blocking_byte_buffer_get(struct adt_blocking_byte_buffer* buffer) {
    semaphore_acquire(buffer->sem);

    spinlock_acquire(&buffer->lock);

    uint8_t c = buffer->buffer[buffer->start_pos];

    buffer->start_pos = (buffer->start_pos + 1) % buffer->total_size;
    buffer->used_size--;

    spinlock_release(&buffer->lock);

    return c;
}

int adt_blocking_byte_buffer_try_get(struct adt_blocking_byte_buffer* buffer, uint8_t* char_out) {
    int status = semaphore_try_acquire(buffer->sem);

    if (status == 0) {
        spinlock_acquire(&buffer->lock);

        *char_out = buffer->buffer[buffer->start_pos];
        
        buffer->start_pos = (buffer->start_pos + 1) % buffer->total_size;
        buffer->used_size--;

        spinlock_release(&buffer->lock);
    }

    return status;
}