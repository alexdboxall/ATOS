
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
    assert((flags & ~(VAS_FLAG_WRITABLE | VAS_FLAG_EXECUTABLE | VAS_FLAG_USER | VAS_FLAG_COPY_ON_WRITE | VAS_FLAG_PRESENT | VAS_FLAG_LOCKED | VAS_FLAG_ALLOCATE_ON_ACCESS)) == 0);
    
    spinlock_acquire(&vas->lock);
    arch_vas_set_entry(vas, virt_addr, phys_addr, flags | VAS_FLAG_USER);
    spinlock_release(&vas->lock);
}


/*
* Modifies the flags on a page. Does not automatically set VAS_FLAG_PRESENT.
*/
void vas_reflag(struct virtual_address_space* vas, size_t virt_addr, int flags)
{
	assert(vas);
		
	assert(virt_addr % ARCH_PAGE_SIZE == 0);
	assert((flags & ~(VAS_FLAG_WRITABLE | VAS_FLAG_EXECUTABLE | VAS_FLAG_USER | VAS_FLAG_COPY_ON_WRITE | VAS_FLAG_PRESENT | VAS_FLAG_LOCKED | VAS_FLAG_ALLOCATE_ON_ACCESS)) == 0);

    spinlock_acquire(&vas->lock);

    int old_flags;
    size_t phys_addr;
    arch_vas_get_entry(vas, virt_addr, &phys_addr, &old_flags);
	arch_vas_set_entry(vas, virt_addr, phys_addr, flags);
    spinlock_release(&vas->lock);
}


/*
* Modifies the flags on a page. Does not automatically set VAS_FLAG_PRESENT.
*/
size_t vas_virtual_to_physical(struct virtual_address_space* vas, size_t virt_addr)
{
	assert(vas);
	assert(virt_addr % ARCH_PAGE_SIZE == 0);

    spinlock_acquire(&vas->lock);

    int old_flags;
    size_t phys_addr;
    arch_vas_get_entry(vas, virt_addr, &phys_addr, &old_flags);
    spinlock_release(&vas->lock);

    return phys_addr;
}


/*
* Unmap a page of virtual memory, and therefore making it so that virtual address can
* no longer be accessed. Returns the old physical address that was mapped, or 0 if none
* was mapped.
*/
size_t vas_unmap(struct virtual_address_space* vas, size_t virt_addr)
{
	assert(vas);
	assert(virt_addr % ARCH_PAGE_SIZE == 0);
	
    spinlock_acquire(&vas->lock);

    /*
    * TODO: how does this go with pages on disk? or ALLOCATE_ON_WRITE, etc.??
    */

    int old_flags;
	size_t old_phys_addr;
	arch_vas_get_entry(vas, virt_addr, &old_phys_addr, &old_flags);
	arch_vas_set_entry(vas, virt_addr, 0, 0);

    spinlock_release(&vas->lock);

	assert(old_phys_addr % ARCH_PAGE_SIZE == 0);

    if (old_flags & VAS_FLAG_COPY_ON_WRITE) {
        panic("TODO: how should vas_unmap work on VAS_FLAG_COPY_ON_WRITE");
    }

    if (!(old_flags & VAS_FLAG_PRESENT) || (old_flags & VAS_FLAG_ALLOCATE_ON_ACCESS)) {
        return 0;
    }

    return old_phys_addr;
}


/*
* Performs a page replacement, and returns the newly freed physical address.
*/
size_t vas_perform_page_replacement(void) {
    struct virtual_address_space* vas = vas_get_current_vas();

    /*
    * Somewhere in the page fault handling code, we need nested spinlocks. This is because
    * handling a page fault may require memory to be allocated, and thus the VAS would already
    * be locked. This is where we will use them. We will not lock again if we are already
    * locked, hence the use of spinlock_acquire_if_unlocked.
    */

    bool needs_unlocking = spinlock_acquire_if_unlocked(&vas->lock);

    /*
    * Find our next victim.
    */
    size_t unlucky_addr = arch_find_page_replacement_virt_address(vas);
    kprintfnv("EVICTING: 0x%X\n", unlucky_addr);

    /*
    * We need to release our lock in order to write to the disk, so lock the page 
    * so nothing happens to it.
    */
    int old_flags;
    size_t phys_addr;
    arch_vas_get_entry(vas, unlucky_addr, &phys_addr, &old_flags);
    kprintfnv("phys_addr: 0x%X\n", phys_addr);
	arch_vas_set_entry(vas, unlucky_addr, phys_addr, old_flags | VAS_FLAG_LOCKED);
    kprintfnv("phys_addr: 0x%X\n", phys_addr);

    /*
    * We don't need to be (spin)locked any longer, as we are only writing out the page, which is
    * already marked as locked.
    */
    if (needs_unlocking) {
        //spinlock_release(&vas->lock);
    }

    /*
    * Save our page onto the disk.
    */
    kprintfnv("about to write: 0x%X\n", unlucky_addr);
    size_t id = swapfile_write((uint8_t*) unlucky_addr);
    kprintfnv("written -> id = 0x%X\n", id);

    /*
    * Reading/writing to page tables again requires us to lock.
    */
    //needs_unlocking = spinlock_acquire_if_unlocked(&vas->lock);

    /*
    * Unmap the page, and then write the swapfile id to the page entry.
    * (Don't override the value of phys_addr, as we need to return it, so use a dummy variable.)
    */
    arch_vas_set_entry(vas, unlucky_addr, id * ARCH_PAGE_SIZE, old_flags & ~(VAS_FLAG_LOCKED | VAS_FLAG_PRESENT));
    kprintfnv("set\n");

    size_t dummy;
    // TODO: what the heck does reading this at this point do???
    arch_vas_get_entry(vas, unlucky_addr, &dummy, &old_flags);
    kprintfnv("got\n");

    if (needs_unlocking) {
        spinlock_release(&vas->lock);
    }

    kprintfnv("repl done\n");
    return phys_addr;
}