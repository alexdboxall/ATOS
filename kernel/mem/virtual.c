
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <physical.h>
#include <virtual.h>
#include <arch.h>
#include <assert.h>
#include <string.h>
#include <spinlock.h>
#include <panic.h>
#include <kprintf.h>
#include <heap.h>

/*
* mem/virtual.c - Virtual Page Allocation
*
* The higher-level half of the virtual memory manager. It is responsible
* for allocating pages in the virtual address space. It acts like the
* physical memory manager, but for virtual pages.
*
*/

struct spinlock virt_lock;

/*
* Each CPU can in theory have different mappings in virtual memory for the
* kernel. However, for simplicity, we will never reallocate kernel virtual
* memory addresses. This makes it much easier to share kernel data, etc.
*/
size_t kernel_sbrk = ARCH_KRNL_SBRK_BASE;

/*
* This function is called once on the bootstrap CPU, and should not call any
* functions from arch. Then each CPU will
* call arch_virt_init(). This can only be used for, e.g. setting up bitmaps
* or tables used to keep track of things.
*/
void virt_init(void)
{
	
}


/*
* Round a number of bytes up to the nearest page boundary.
*/
size_t virt_bytes_to_pages(size_t bytes) {
	return (bytes + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
}

/*
* Literally just needs to pluck addresses out of thin air, so long as they
* don't conflict with each other, and are between ARCH_KRNL_SBRK_BASE and
* ARCH_KRNL_SBRK_LIMIT. 
*
* The current implementation is very simple and does not allow freeing.
*/ 
size_t virt_allocate_krnl_region(size_t bytes)
{
	assert(bytes != 0);

	spinlock_acquire(&virt_lock);

	if (kernel_sbrk >= ARCH_KRNL_SBRK_LIMIT) {
		panic("kernel sbrk limit reached");
	}

	size_t old_sbrk = kernel_sbrk;
	kernel_sbrk += virt_bytes_to_pages(bytes) * ARCH_PAGE_SIZE;

	spinlock_release(&virt_lock);

	return old_sbrk;
}

void virt_deallocate_krnl_region(size_t virt_addr)
{
	/*
	* Do nothing for now.
    * TODO: probably scan thru region checking what's there, freeing any phys pages marked as allocated
	*/
	(void) virt_addr;
}


/*
* Map physical memory to a region of physical memory. Any higher-level memory
* functions (such as the heap) should be built on top of this.
*/
size_t virt_allocate_pages(size_t pages, int flags) {
	assert(pages != 0);
	
	size_t virt_addr = virt_allocate_krnl_region(pages * ARCH_PAGE_SIZE);

	for (size_t i = 0; i < pages; ++i) {
		vas_map(vas_get_current_vas(), phys_allocate_page(), virt_addr + i * ARCH_PAGE_SIZE, flags);

		/*
		* Fill memory with '0xDEADBEEF', which is an easy value to see when debugging.
		*/
		for (int j = 0; j < ARCH_PAGE_SIZE / 4; ++j) {
			*(uint32_t*)(virt_addr + i * ARCH_PAGE_SIZE + j * 4) = 0xDEADBEEF;
		}
	}

	vas_flush_tlb();

	return virt_addr;
}

void virt_free_pages(size_t virt_addr) {
	// TODO: we need to deallocate the physical pages (and work out how long the range is...)

    kprintf("virt free\n");
	
	virt_deallocate_krnl_region(virt_addr);
}