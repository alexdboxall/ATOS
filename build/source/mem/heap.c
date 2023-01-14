
#include <heap.h>
#include <virtual.h>
#include <physical.h>
#include <arch.h>
#include <panic.h>
#include <spinlock.h>
#include <kprintf.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

/* 
* heap.h - Kernel Heap
* 
* The heap (malloc/free) is used for dynamic memory allocation within the kernel. 
* Note that these functions will allocate kernel memory, so they cannot be used
* for application heaps (that should be implemented in a userspace library). 
*/

// must fit into *virtual* memory
#define MAX_HEAP_SIZE                   (1024 * 1024 * 256)

#define BLOCK_METADATA_BYTES            (sizeof(size_t) * 2)
#define BLOCK_MINIMUM_BYTES             (sizeof(size_t) * 4)

#define BLOCK_RETURN_OFFSET_IN_WORDS    1             // skip the size/allocated word

#define BLOCK_SIZE_INDEX                0
#define BLOCK_NEXT_INDEX                1
#define BLOCK_PREV_INDEX                2

#define BLOCK_ALLOCATED                 1

#define GET_BLOCK_SIZE(blk)             ((blk[BLOCK_SIZE_INDEX] & ~BLOCK_ALLOCATED))
#define IS_BLOCK_ALLOCATED(blk)         ((blk[BLOCK_SIZE_INDEX] & BLOCK_ALLOCATED))
#define IS_BLOCK_FREE(blk)              ((!IS_BLOCK_ALLOCATED(blk)))

/*
* Block layout:
*
* word 0:   size of block in bytes, low bit set if allocated
* word 1:   (of a free block) next free block
* word 2:   (of a free block) prev free block
*
* word n:   copy of word 0
*/

size_t* block_head;

bool full_heap_initialised = false;

uint8_t bootstrap_heap[8192];
size_t bootstrap_heap_pos = 0;

struct spinlock heap_lock;

bool full_heap_initialised;

void heap_init(void)
{
	spinlock_init(&heap_lock, "heap lock");	
    full_heap_initialised = false;
}	

void heap_reinit(void) {
    spinlock_acquire(&heap_lock);

    size_t heap_address = virt_allocate_unbacked_krnl_region(MAX_HEAP_SIZE);

    for (size_t i = 0; i < MAX_HEAP_SIZE / ARCH_PAGE_SIZE; ++i) {
        vas_reflag(vas_get_current_vas(), heap_address + i * ARCH_PAGE_SIZE, VAS_FLAG_ALLOCATE_ON_ACCESS | VAS_FLAG_LOCKED);
    }

    block_head = (size_t*) heap_address;

    /*
    * We want to put allocated blocks on either end of the memory range so we never try
    * to coalesce with memory that doesn't belong to us.
    */
    block_head[BLOCK_SIZE_INDEX] = BLOCK_MINIMUM_BYTES | BLOCK_ALLOCATED;
    block_head[BLOCK_MINIMUM_BYTES / sizeof(size_t) - 1] = block_head[BLOCK_SIZE_INDEX];

    size_t* end_allocation_start = block_head + ((MAX_HEAP_SIZE - BLOCK_MINIMUM_BYTES) / sizeof(size_t));
    end_allocation_start[BLOCK_SIZE_INDEX] = BLOCK_MINIMUM_BYTES | BLOCK_ALLOCATED;
    end_allocation_start[BLOCK_MINIMUM_BYTES / sizeof(size_t) - 1] = end_allocation_start[BLOCK_SIZE_INDEX];
    
    /*
    * Now for the rest of memory.
    */
    size_t* main_memory_start = block_head + (BLOCK_MINIMUM_BYTES / sizeof(size_t));
    main_memory_start[BLOCK_SIZE_INDEX] = MAX_HEAP_SIZE - BLOCK_MINIMUM_BYTES * 2;
    main_memory_start[BLOCK_NEXT_INDEX] = 0;
    main_memory_start[BLOCK_PREV_INDEX] = 0;
    main_memory_start[(MAX_HEAP_SIZE - BLOCK_MINIMUM_BYTES * 2) / sizeof(size_t) - 1] = main_memory_start[BLOCK_SIZE_INDEX];

    block_head = main_memory_start;

    full_heap_initialised = true;
    spinlock_release(&heap_lock);
}

size_t heap_used = 0;

/*
* Finds a free block that is of a given size, or greater. The given size should include 
* the metadata size. If no block can be found, it returns NULL (probably a good time for
* a kernel panic).
*/
static size_t* find_free_block(size_t min_size_with_metadata) {
    size_t* current = block_head;

    while (current != NULL) {
        assert(IS_BLOCK_FREE(current));

        size_t block_size = GET_BLOCK_SIZE(current);

        if (block_size >= min_size_with_metadata) {
            return current;
        }
        
        /* It is too small, so go to the next block. */
        current = (size_t*) current[BLOCK_NEXT_INDEX];
    }

    panic("find_free_block: kernel heap exhausted");

    return NULL;
}

/*
static void print_blocks(void) {
    size_t* current = block_head;

    while (current != NULL) {
        size_t block_size = GET_BLOCK_SIZE(current);
        if (current != block_head) {
            kprintf(", ");
        }
        kprintf("%d", block_size);

        current = (size_t*) current[BLOCK_NEXT_INDEX];
    }

    kprintf("\n");
}*/

static void set_block_size(size_t* block, size_t new_size, bool allocated) {
    assert(new_size % sizeof(size_t) == 0);

    block[BLOCK_SIZE_INDEX] = new_size | (allocated ? BLOCK_ALLOCATED : 0);
    block[new_size / sizeof(size_t) - 1] = block[BLOCK_SIZE_INDEX];
}

void* malloc(size_t size)
{
    assert(size > 0);

    spinlock_acquire(&heap_lock);

    /*
    * All later calculations and function calls require the metadata size
    * to be included. We must also round up to the word size.
    */
	size = (size + BLOCK_METADATA_BYTES + 3) & ~0x3;

    if (!full_heap_initialised) {
        size_t pos = bootstrap_heap_pos;
        bootstrap_heap_pos += size;
        spinlock_release(&heap_lock);
        return (void*) (bootstrap_heap + pos);
    }

    /*
    * Find a block that will fit us. 
    */
    size_t* block = find_free_block(size);
    size_t block_size = GET_BLOCK_SIZE(block);

    size_t* prev_block = (size_t*) block[BLOCK_PREV_INDEX];
    size_t* next_block = (size_t*) block[BLOCK_NEXT_INDEX];

    if (size == block_size || size + BLOCK_MINIMUM_BYTES > block_size) {
        /* 
        * Remove this block entirely (either we need the entire area, or
        * it would cause the resulting block to be too small.
        *
        * Mark as allocated, and use the entire size, so that the entire
        * thing gets freed if there was a small amount of extra.
        */
        block[BLOCK_SIZE_INDEX] = block_size | BLOCK_ALLOCATED;
        block[block_size / sizeof(size_t) - 1] = block[BLOCK_SIZE_INDEX];

        /* Fix up the double linked list. */
        if (prev_block) {
            prev_block[BLOCK_NEXT_INDEX] = (size_t) next_block;

        } else {
            /* 
            * We removed the head! If that was the only block, we need to 
            * allocate another so we still have a head.
            */
            block_head = next_block;
            if (block_head == NULL) {
                panic("malloc: kernel heap exhausted");
            }
        }

        if (next_block) {
            next_block[BLOCK_PREV_INDEX] = (size_t) prev_block;
        }

    } else {
        /*
        * Mark as allocated, and update the size information.
        */
        set_block_size(block, size, true);

        /*
        * As block is a size_t*, but size is in bytes, we need to convert size to words.
        */
        size_t* subdivided_block = block + (size / sizeof(size_t)); 
        size_t subdivided_block_size = block_size - size;

        /*
        * Set the size correctly in the subdivided block.
        */
        set_block_size(subdivided_block, subdivided_block_size, false);

        /*
        * Copy over the next and previous pointers.
        */
        subdivided_block[BLOCK_NEXT_INDEX] = (size_t) next_block;
        subdivided_block[BLOCK_PREV_INDEX] = (size_t) prev_block;

        /*
        * Fix up the double linked list to make them point to us.
        */
        if (prev_block) {
            prev_block[BLOCK_NEXT_INDEX] = (size_t) subdivided_block;

        } else {
            /* 
            * We removed the head! The head is now the subdivided block.
            */
            block_head = subdivided_block;
        }

        if (next_block) {
            next_block[BLOCK_PREV_INDEX] = (size_t) subdivided_block;
        }
    }

    heap_used += size;

	spinlock_release(&heap_lock);

    return (void*) (block + BLOCK_RETURN_OFFSET_IN_WORDS);
}

static void make_block_root(size_t* new_root) {
    assert(IS_BLOCK_FREE(new_root));

    new_root[BLOCK_NEXT_INDEX] = (size_t) block_head;
    new_root[BLOCK_PREV_INDEX] = 0;

    if (block_head != NULL) {
        block_head[BLOCK_PREV_INDEX] = (size_t) new_root;
    }

    block_head = new_root;
}

static void remove_block_from_list(size_t* block) {
    size_t* next = (size_t*) block[BLOCK_NEXT_INDEX];
    size_t* prev = (size_t*) block[BLOCK_PREV_INDEX];

    if (prev != NULL) {
        prev[BLOCK_NEXT_INDEX] = (size_t) next;

    } else {
        /*
        * Removing the head, so need to adjust block_head.
        */
        block_head = next;
    }

    if (next != NULL) {
        next[BLOCK_PREV_INDEX] = (size_t) prev;
    }
}

void free(void* ptr)
{
    /*
    * Memory on the bootstrap heap cannot be freed.
    */
    if (ptr >= (void*) bootstrap_heap && ptr < (void*) (bootstrap_heap + bootstrap_heap_pos)) {
        return;
    }

	size_t* block = ((size_t*) ptr) - BLOCK_RETURN_OFFSET_IN_WORDS;
    size_t size = GET_BLOCK_SIZE(block);

    size_t* prev_block = block - (*(block - 1) & ~BLOCK_ALLOCATED) / sizeof(size_t);
    size_t* next_block = block + size / sizeof(size_t);

    bool coalesce_left = IS_BLOCK_FREE(prev_block); 
    bool coalesce_right = IS_BLOCK_FREE(next_block); 

    if (!coalesce_left && !coalesce_right) {
        /*
        * Case 1 - insert this block as the new root
        */

        block[BLOCK_SIZE_INDEX] &= ~BLOCK_ALLOCATED;
        make_block_root(block);

    } else if (!coalesce_left && coalesce_right) {
        /* 
        * Case 2 - remove successor block from list, coalesce both blocks,
        *          then add the combined block as the root
        */

        size_t combined_size = size + GET_BLOCK_SIZE(next_block);

        remove_block_from_list(next_block);
        set_block_size(block, combined_size, false);
        make_block_root(block);

    } else if (coalesce_left && !coalesce_right) {
        /* 
        * Case 3 - remove predecessor block from list, coalesce both blocks,
        *          then add the combined block as the root
        */

        size_t combined_size = size + GET_BLOCK_SIZE(prev_block);

        remove_block_from_list(prev_block);
        set_block_size(prev_block, combined_size, false);
        make_block_root(prev_block);

    } else {
        /* 
        * Case 4 - remove predecessor and successor blocks from list, coalesce
        *          all three blocks, then add the combined block as the root
        */

        size_t combined_size = size + GET_BLOCK_SIZE(prev_block) + GET_BLOCK_SIZE(next_block);

        remove_block_from_list(prev_block);
        remove_block_from_list(next_block);
        set_block_size(prev_block, combined_size, false);
        make_block_root(prev_block);
    }

    heap_used -= size;
}


void* realloc(void* ptr, size_t size) {
    /* 
    * TODO: this is REALLY BAD, but it might work for now...
    */
    void* new_ptr = malloc(size);
    memcpy(new_ptr, ptr, size);
    free(ptr);
    return new_ptr;
}