
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <physical.h>
#include <arch.h>
#include <assert.h>
#include <string.h>
#include <spinlock.h>
#include <panic.h>
#include <kprintf.h>
#include <virtual.h>
#include <bitarray.h>
#include <thread.h>

/*
* mem/physical.c - Physical Memory Manager
*
* Allocating and freeing physical memory. These functions
* work on a single page at a time - the virtual memory manager
* can map pages together so that larger memory chunks can be used.
* 
* These functions can't use the rest of the memory subsystem as they
* are the foundation the rest of it is built on top of. Therefore, we
* will use a fixed sized array to keep track of what pages have been
* allocated. As we only allocate and free one page at at time, a simple
* bitmap structure where each bit corresponds to a page is sufficient.
*/

/* 
* This should be a power of 2, and above any reasonable
* page size a machine might have. 
* TODO: move to arch
*/
#define MAX_KILOBYTES_OF_MEMORY (1024 * 128)
#define MAX_PAGES				(MAX_KILOBYTES_OF_MEMORY / ARCH_PAGE_SIZE * 1024)
#define ALLOCATION_BITMAP_SIZE	(MAX_PAGES / 8)

/*
* The allocation bitmap. Each bit represents a page according to the formula:
* 
*	page = bitnum + bytenum * 8			(bitnum is from 0-7, with 0 being the LSB)
* 
* The pages will be tracked from the start of physical memory (address 0).
* A clear bit means the page is unavailable - either it doesn't exist or has been
* allocated, and a set bit means it is free for allocation
*/
uint8_t page_allocation_bitmap[ALLOCATION_BITMAP_SIZE];

struct spinlock phys_lock;

static void phys_mark_as_free(size_t page_num)
{
	assert(page_num < MAX_PAGES);
	assert(spinlock_is_held(&phys_lock));

    bitarray_set(page_allocation_bitmap, page_num);
}

static void phys_mark_as_used(size_t page_num)
{
	assert(page_num < MAX_PAGES);
	assert(spinlock_is_held(&phys_lock));

    bitarray_clear(page_allocation_bitmap, page_num);
}

static bool phys_is_page_free(size_t page_num)
{
	assert(page_num < MAX_PAGES);
	assert(spinlock_is_held(&phys_lock));

    return bitarray_is_set(page_allocation_bitmap, page_num);
}

int num_pages_used = 0;
int num_pages_total = 0;

void phys_init(void)
{
	spinlock_init(&phys_lock, "physical memory lock");
	spinlock_acquire(&phys_lock);

	/*
	* We don't know what memory exists yet, so all of it must be marked as unusable.
	*/
	memset(page_allocation_bitmap, 0, sizeof(page_allocation_bitmap));

	/*
	* Now we can scan the memory tables and fill in the memory that is there.
	*/
	for (int i = 0; true; ++i) {
		struct arch_memory_range* range = arch_get_memory(i);

		if (range == NULL) {
			/* No more memory exists */
			break;

		} else {
			/* 
			* Round conservatively (i.e., round the first page up, and the last page down)
			* so we don't accidentally allow non-existant memory to be allocated.
			*/
			size_t first_page = (range->start + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
			size_t last_page = (range->start + range->length) / ARCH_PAGE_SIZE;

			while (first_page < last_page && first_page < MAX_PAGES) {
                kprintf("can use 0x%X\n", first_page * ARCH_PAGE_SIZE);
				phys_mark_as_free(first_page++);
				++num_pages_total;
			}
		}
	}

	spinlock_release(&phys_lock);
}



/*
* Must not be called while the scheduler lock is held. This is because a page fault will
* cause the disk driver to try to acquire a semaphore - which in turn needs the scheduler
* lock to be clear.
*
* OR: modify something so semaphores can check if in page fault handler, and if the lock
*     is already held, then don't bother locking/unlocking it (assuming noone tries to 
*     allocate a page with any thread lists in an inconsistent state, as then the semaphore
*     acquire will corrupt it).
*/
size_t phys_allocate_page(void)
{
	spinlock_acquire(&phys_lock);

	++num_pages_used;

	/*
	* We will start by looking at the page after the one we just allocated
	* This way we can avoid searching for a free page from the start of the
	* bitmap each time. Hence the static variable.
	*/
	static size_t page_num = 0;

	for (int i = 0; i < MAX_PAGES; ++i) {
		/* 
		* Doing this at the start of the loop means that next time this function
		* is called, we will look at the next page instead of looking at the same
		* page again. Page 0 will get used after it loops back to the start.
		*/
		page_num = (page_num + 1) % MAX_PAGES;

		if (phys_is_page_free(page_num)) {
			phys_mark_as_used(page_num);
			spinlock_release(&phys_lock);
			return page_num * ARCH_PAGE_SIZE;
		}
    } 

	spinlock_release(&phys_lock);

	/*
	* No pages left. Stick one on the disk.
	*/
    size_t ret = vas_perform_page_replacement();
	return ret;
}

void phys_free_page(size_t phys_addr)
{
	spinlock_acquire(&phys_lock);

	--num_pages_used;

	size_t page_num = phys_addr / ARCH_PAGE_SIZE;
	
	assert(phys_is_page_free(page_num));
	phys_mark_as_free(page_num);

	spinlock_release(&phys_lock);
}