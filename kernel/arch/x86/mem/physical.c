
#include <stdint.h>
#include <stddef.h>
#include <arch.h>
#include <assert.h>
#include <panic.h>
#include <machine/mem/virtual.h>
#include <kprintf.h>

/*
* x86/mem/physical.c - Physical Memory Detection
*
* We need to detect what physical memory exists on the system so it can
* actually be allocated. When the bootloader runs, it puts a pointer to
* a table in EBX. This table then contains a pointer to a memory table
* containing the ranges of memory, and whether or not they are available for use.
*/

/*
* An entry in the memory table that GRUB loads.
*/
struct memory_table_entry
{
	uint32_t size;
	uint32_t addr_low;
	uint32_t addr_high;
	uint32_t len_low;
	uint32_t len_high;
	uint32_t type;
	
} __attribute__((packed));

/*
* We can only use memory if the type (see the struct above) is this.
*/
#define MULTIBOOT_MEMORY_AVAILABLE  1

/*
* A pointer to the main GRUB table. Defined and set correctly in
* x86/lowlevel/kernel_entry.s
*/
extern uint32_t* x86_grub_table;

/*
* A pointer to the memory table found in the main GRUB table.
*/
struct memory_table_entry* memory_table = NULL;

struct arch_memory_range* arch_get_memory(int index)
{
	static int prev_index = -1;
	static struct arch_memory_range range;
	static int bytes_used = 0;
	static int table_length = 0;
	
	/*
	* For when assertions are off
	*/
	(void) index;
	
	/*
	* We should be iterating the table in order.
	*/
	assert(index == prev_index + 1);
	prev_index++;
	
retry:
	/*
	* If this is the first time we are called, we need to find the address of
	* the memory table in the main table.
	*/
	if (memory_table == NULL) {
		x86_grub_table = (uint32_t*) lowmem_physical_to_virtual((size_t) x86_grub_table);
		
		/*
		* A quick check to ensure that the table is somewhat valid.
		*/
		uint32_t flags = x86_grub_table[0];
		if (!(flags >> 6) & 1) {
			panic("No memory map from GRUB");
		}
		
		table_length = x86_grub_table[11];
		memory_table = (struct memory_table_entry*) lowmem_physical_to_virtual(x86_grub_table[12]);
	}

	/*
	* No more memory, we have reached the end of the table
	*/
	if (bytes_used >= table_length) {
		return NULL;
	}
	
	/*
	* Start reading the memory table into the range.
	* 
	* If the high half of the length is non-zero, we have at least 4GB of memory in
	* this range. We can't handle any more than 4GB, so just make it a 4GB range.
	*/
	size_t type = memory_table->type;
	range.start = memory_table->addr_low;
	range.length = memory_table->len_high ? 0xFFFFFFFFU : memory_table->len_low;
	++memory_table;
	bytes_used += sizeof(struct memory_table_entry);
	
	extern size_t _kernel_end;
	size_t max_kernel_addr = (((size_t) &_kernel_end) - 0xC0000000 + 0xFFF) & ~0xFFF;

	/*
	* Don't allow the use of non-RAM, or addresses completely below the kernel.
	*/
	if (type != MULTIBOOT_MEMORY_AVAILABLE || range.start + range.length < max_kernel_addr) {
		goto retry;
	}

	/*
	* If it starts below the kernel, but ends above it, cut it off so only the
	* part above the kernel is used.
	*/
	if (range.start < max_kernel_addr) {
		range.length = range.start + range.length - max_kernel_addr;
		range.start = max_kernel_addr;
	}
	
	return &range;
}