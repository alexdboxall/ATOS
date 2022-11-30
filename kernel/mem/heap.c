
#include <heap.h>
#include <virtual.h>
#include <physical.h>
#include <arch.h>
#include <spinlock.h>
#include <kprintf.h>
#include <stddef.h>
#include <assert.h>

/* 
* heap.h - Kernel Heap
* 
* The heap (malloc/free) is used for dynamic memory allocation within the kernel. 
* Note that these functions will allocate kernel memory, so they cannot be used
* for application heaps (that should be implemented in a userspace library). 
*
* Note that this implementation is shit. It is enough to work for now, but it is
* hugely inefficient. It will not free anything, and will waste 'heaps' (*haha*)
* of memory.
*
* This really should be the first thing you should rewrite. 
*/

struct spinlock heap_lock;

void heap_init(void)
{
	spinlock_init(&heap_lock, "heap lock");	
}	

void* malloc(size_t size)
{
	/*
	* If we are allocating one page or more, just allocate that number of pages. If we need
	* less than page, try to fit it into the previous page that was allocated. Otherwise, just
	* allocate a new one. This system is extremely bad, as it creates a huge amount of fragmentation
	* and doesn't allow freeing, but it is good enough for now.
	*/

	assert(size != 0);

	spinlock_acquire(&heap_lock);

	static size_t previous_page = 0;
	static size_t previous_page_offset = 0;

	void* ret;

	/* Round up to the nearest 16 bytes */
	size = (size + 15) & ~0xF;

	if (size < ARCH_PAGE_SIZE) {
		if (previous_page != 0 && size <= ARCH_PAGE_SIZE - previous_page_offset) {
			/* We can fit it in the previous page */
			ret = (void*)(previous_page + previous_page_offset);
			previous_page_offset += size;
			assert(previous_page_offset <= ARCH_PAGE_SIZE);

		} else {
			/* We need a new page */
			previous_page = virt_allocate_pages(1, VAS_FLAG_WRITABLE);
			previous_page_offset = size;
			ret = (void*) previous_page;
		}
		
	} else {
		ret = (void*) virt_allocate_pages(virt_bytes_to_pages(size), VAS_FLAG_WRITABLE);	
	}

	spinlock_release(&heap_lock);
	return ret;
}

void free(void* ptr)
{
	/*
	* Using our current allocation algorithm, we can't free memory.
	*/

	(void) ptr;
}