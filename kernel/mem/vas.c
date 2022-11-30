
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <physical.h>
#include <virtual.h>
#include <arch.h>
#include <cpu.h>
#include <assert.h>
#include <string.h>
#include <spinlock.h>
#include <panic.h>
#include <kprintf.h>
#include <heap.h>

/*
* mem/vas.c - Virtual Address Spaces
*
* The lower-level half of the virtual memory manager. It provides functions
* for creating, destroying, loading, and mapping pages in and out of address spaces.
*
*/

struct virtual_address_space* vas_create(void)
{
	struct virtual_address_space* vas = (struct virtual_address_space*) malloc(sizeof(struct virtual_address_space));
	vas->copied_from = NULL;
    spinlock_init(&vas->lock, "per-vas lock");
	arch_vas_create(vas);
	return vas;
}

struct virtual_address_space* vas_get_current_vas(void)
{
	return current_cpu->current_vas;
}

/*
* Informs the CPU that paging structures have been modified, and to flush any
* internal cache if it is necessary.
*/
void vas_flush_tlb(void) {
	arch_flush_tlb();
}

/*
* Free a virtual address space.
*/
void vas_destroy(struct virtual_address_space* vas)
{
	assert(vas);

    spinlock_acquire(&vas->lock);

	/*
	* We cannot destroy the address space we are currently using, otherwise the 
	* system will try to use invalid addresses for everything (likely causing the
	* computer to crash or reboot.
	*/
	if (current_cpu->current_vas == vas) {
		panic("attempting to destroy the current address space");
	}

	arch_vas_destroy(vas->data);
    spinlock_release(&vas->lock);
	free(vas);
}

/*
* Sets a virtual address space to be the current one. All memory accesses
* will go through this address space.
*/
void vas_load(struct virtual_address_space* vas)
{
	assert(vas);

    spinlock_acquire(&vas->lock);

	arch_vas_load(vas->data);
	current_cpu->current_vas = vas;

    spinlock_release(&vas->lock);
}

/*
* Creates a complete copy of an address space and all of the data within it.
*/
struct virtual_address_space* vas_copy(struct virtual_address_space* original)
{    
	assert(original);

	struct virtual_address_space* copy = (struct virtual_address_space*) malloc(sizeof(struct virtual_address_space));
    spinlock_init(&copy->lock, "per-vas lock");

    spinlock_acquire(&original->lock);

	arch_vas_copy(original, copy);
    copy->copied_from = original;

    spinlock_release(&original->lock);

	return copy;
}

/*
* Map a page of virtual memory to a physical memory page.
*/
void vas_map(struct virtual_address_space* vas, size_t phys_addr, size_t virt_addr, int flags)
{
	assert(vas);
	
	flags |= VAS_FLAG_PRESENT;
	
	assert(phys_addr % ARCH_PAGE_SIZE == 0);
	assert(virt_addr % ARCH_PAGE_SIZE == 0);
	assert((flags & ~(VAS_FLAG_WRITABLE | VAS_FLAG_EXECUTABLE | VAS_FLAG_USER | VAS_FLAG_COPY_ON_WRITE | VAS_FLAG_PRESENT | VAS_FLAG_LOCKED)) == 0);

    spinlock_acquire(&vas->lock);
	arch_vas_set_entry(vas, virt_addr, phys_addr, flags);
    spinlock_release(&vas->lock);
}

/*
* Unmap a page of virtual memory, and therefore making it so that virtual address can
* no longer be accessed. Returns the old physical address that was mapped.
*/
size_t vas_unmap(struct virtual_address_space* vas, size_t virt_addr)
{
	assert(vas);
	assert(virt_addr % ARCH_PAGE_SIZE == 0);
	
    spinlock_acquire(&vas->lock);

	size_t old_phys_addr = 0xDEAD0000;
	arch_vas_set_entry(vas, virt_addr, 0, 0);

    spinlock_release(&vas->lock);

	assert(old_phys_addr % ARCH_PAGE_SIZE == 0);

	/*
	* TODO: vas_unmap needs to return the old physical address
	*/
	kprintf("TODO: vas_unmap return value\n");

	return old_phys_addr;
}
