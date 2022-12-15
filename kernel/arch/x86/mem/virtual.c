
#include <machine/mem/virtual.h>
#include <stddef.h>
#include <arch.h>
#include <assert.h>
#include <physical.h>
#include <virtual.h>
#include <panic.h>
#include <string.h>
#include <heap.h>
#include <cpu.h>
#include <spinlock.h>
#include <errno.h>
#include <kprintf.h>

/*
* x86/mem/virtual.c - x86 Virtual Memory 
*
* Functions for dealing with virtual memory and address spaces on the x86.
* 
* The x86 paging structure has two layers, the page directory, and the page tables.
* Both page tables and the page directory contain an almost identical structure.
* Each are a single page long (4KB), and contain 1024 entries that are 4 bytes long
* each. In the page directory, these entries point to the physical address of a
* page table. In a page table, these entires point to a physical address of a 4KB page
* of RAM. Therefore, a page table can map 4MB of RAM, and the page directory can map 4GB
* of RAM. This is the entire 32-bit address space. Hence, we just load the physical
* address of the page directory into a specical control register, CR3. We can change
* virtual address spaces by putting a different address into CR3.
*
*	 CR3			Page			  Page Tables
* 				  Directory				_______
*   		    			     ----> |_______| ----> 4KB of physical RAM
*  _______         _______		/	   |_______| ----> 4KB of physical RAM
* |_______|  ---> |_______|  ---       |_______| ----> 4KB of physical RAM
*                 |_______|  --        |_______| ----> ...
*                 |_______|    \		  etc.
*                 |_______|     \       _______
*					 etc.         ---> |_______| ----> ...
*									   |_______| ----> ...
*									   |_______| ----> ...
*									   |_______| ----> ...
*									   	  etc.				
*
* An example for the mapping between virtual and physical memory. Let's walk through
* an access to memory address 0x12345678.
*
* Each page table maps 4MB (0x400000) of memory. So:
*
*		page table num.			= 0x12345678 / 0x400000				= 0x48	(72)
*		page table entry. num.	= (0x12345678 % 0x400000) / 0x1000	= 0x345	(837)
*		offset within page 		= (0x12345678 % 0x400000) % 0x1000	= 0x678
*
* So we want to find the 72nd page table. The address of this can be found in the 72nd
* entry of the page directory. Then we look at the 837th entry in that page table. 
* This entry contains the physical memory address of the page (for this example, let's
* say it is 0xABCDE000). We then add 0x678 to get the offset in the page. 
* Hence virtual address 0x12345678 is mapped to physical address 0xABCDE678.
*
* Note that the CPU had to read from memory multiple times to convert virtual to physical.
* This happens on every single memory access we do! Hence the CPU contains a cache of
* the page directory/tables called the TLB. We need to flush the TLB when we modify
* page tables to ensure we don't keep using outdated page mappings.
* 
* Because all page addresses are a multiple of 4KB, the bottom 12 bits of each 
* entry would be 0. This allows for some flags to be placed in the bottom 12 bits.
* The lowest 9 or so bits have a use in the CPU (e.g. read-only pages, usermode pages),
* but the highest 3 bits are free for use by the OS for bookkeeping.
*/

#define x86_PAGE_PRESENT				(1 << 0)
#define x86_PAGE_WRITABLE				(1 << 1)
#define x86_PAGE_USER					(1 << 2)
#define x86_PAGE_LOCKED					(1 << 8)
#define x86_PAGE_ALLOCATE_ON_ACCESS		(1 << 10)
#define x86_PAGE_COPY_ON_WRITE			(1 << 11)

#define KERNEL_VIRT_ADDR				0xC0000000
#define RECURSIVE_MAPPING_ADDR			0xFFC00000
#define RECURSIVE_MAPPING_ALT_ADDR		0xFF800000

#define PAGE_SIZE						4096

/*
* NOTE: for the write/user flags to work, it must be set in BOTH the directory
*       and the table. Hence we will always set the write flag in the directory,
*       and user flag if we are below 0xC0000000
*
*/

/*
* Set the CR3 control register. Defined in x86/mem/virtual.s
*/
extern void x86_set_cr3(size_t);

/*
* We need to keep track of the page directory's physical address so we can
* tell the CPU about it (by loading CR3), and the virtual address so we can
* actually access the table. All other pages can be found using the entries
* in the page directory and the recursive paging trick.
*
* Keep them in this order - the assembly code requires it.
*/
struct x86_vas
{
	size_t page_dir_phys;
	size_t page_dir_virt;
};

/*
* To initialise page directories/tables were need to write them, but doing
* so requires them to be mapped into virtual memory. Using the virtual memory
* manager's allocation features creates a catch-22, so we need to use virtual 
* memory that is already mapped (e.g. the kernel data).
*/
size_t temp_virtual_page[1024] __attribute__((aligned(PAGE_SIZE)));

/*
* The page directory used the kernel, and the page table responsible for mapping
* the kernel data. As these are used to initialise the virtual memory manager, for
* the reasons above they also have to already be in virtual memory.
*/ 
size_t kernel_page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
size_t first_page_table[1024] __attribute__((aligned(PAGE_SIZE)));

struct virtual_address_space kernel_vas[ARCH_MAX_CPU_ALLOWED];
struct x86_vas kernel_vas_x86[ARCH_MAX_CPU_ALLOWED];

/*
* Converts a physical address guaranteed to be less than 4MB to its
* virtual address. This is trivial as we map the low 4MB to KERNEL_VIRT_ADDR.
*/
size_t lowmem_physical_to_virtual(size_t physical)
{
	assert(physical < 0x400000);
	return 0xC0000000 + physical;
}

/*
* A helper function to fill in a missing page table. The page directory can
* contain non-present page tables, and if we need to map a page in one of these
* non-present ranges, we must first make the page table itself present by
* creating one.
*/
static void allocate_page_table(struct virtual_address_space* vas, size_t* page_dir, int entry_num) {
    kprintf("caller: 0x%X\n", __builtin_return_address(0));

    assert(entry_num >= 0 || entry_num < 1024);
    assert(spinlock_is_held(&vas->lock));

	size_t p_addr = phys_allocate_page();
	size_t v_addr = (size_t) temp_virtual_page;
    
	/*
	* In order to clear the page, we need to temporarily map it into memory.
	*/
    arch_vas_set_entry(vas, v_addr, p_addr, VAS_FLAG_WRITABLE | VAS_FLAG_PRESENT | VAS_FLAG_LOCKED);

    vas_flush_tlb();
    memset((void*) v_addr, 0, 4096);
    arch_vas_set_entry(vas, v_addr, p_addr, 0);

	// Add it to the page directory
	page_dir[entry_num] = p_addr | x86_PAGE_PRESENT | x86_PAGE_LOCKED | x86_PAGE_WRITABLE | (entry_num < 768 ? x86_PAGE_USER : 0);
    vas_flush_tlb();
}

static void setup_current_cpu() {
	/*
	* We also need to setup the current_cpu structure. The virtual address
	* should be the same regardless of the CPU, but it should be mapped to
	* different physical addresses on each CPU
	*/
	
	assert(sizeof(struct cpu) <= ARCH_PAGE_SIZE);
	
	/*
	* Choose the 'global' virtual address. Only should be done the first
	* time, as they should share it.
	*/
	if (cpu_get_count() == 0) {
		current_cpu = (struct cpu*) virt_allocate_unbacked_krnl_region(sizeof(struct cpu));
	}

	/*
	* Map in a 'local' physical address
	*/
	vas_map(&kernel_vas[cpu_get_count()], phys_allocate_page(), (size_t) current_cpu, VAS_FLAG_WRITABLE | VAS_FLAG_LOCKED);

	current_cpu->current_vas = &kernel_vas[cpu_get_count()];
	current_cpu->cpu_number = cpu_get_count();
}

/*
* Initialises a virtual address space for the kernel. It is not able to use any
* functions in the virtual memory manager as it has not yet been initialised.
*
* It maps the low 4MB of physical memory to KERNEL_VIRT_ADDR, and sets up the 
* recursive mapping system.
*
* Note that all kernel virtual address spaces shares the same page directory.
* This is so that we have a consistent kernel state in all processes.
*/
void x86_per_cpu_virt_initialise(void)
{
	memset(kernel_page_directory, 0, PAGE_SIZE);

    extern size_t _kernel_end;
	size_t max_kernel_addr = (((size_t) &_kernel_end) + 0xFFF) & ~0xFFF;

	/*
	* Map the kernel by mapping the first 1MB + kernel size up to 0xC0000000 (assumes the kernel is 
    * less than 4MB). This needs to match what kernel_entry.s exactly.
    * 
    * We need to ensure all the memory actually exists (the disk swapper will hate you if you refer 
    * to memory that doesn't actually exist). Therefore, we just map the first 1.5MB (meaning the kernel
    * can be up to 0.5MB in size, as it loads at 1MB).
    * 
    * We must lock the entire section. We can never swap out a page table, as when we swap it back in
    * back stuff happens (it gets filled with 0xDEADBEEF, and then because it's a page table, *those*
    * 0xDEADBEEFs are seen as pages by the code that searches for which page to switch out.)
	*/
    kprintf("max_kernel_addr = 0x%X\n", max_kernel_addr);

    size_t num_pages = (max_kernel_addr - 0xC0000000) / PAGE_SIZE;

	kernel_page_directory[768] = ((size_t) first_page_table - KERNEL_VIRT_ADDR) | x86_PAGE_PRESENT | x86_PAGE_WRITABLE | x86_PAGE_USER | x86_PAGE_LOCKED;
	/* <= is required to make it match kernel_entry.s */
    for (size_t i = 0; i < num_pages; ++i) {
		first_page_table[i] = (i * PAGE_SIZE) | x86_PAGE_PRESENT | x86_PAGE_LOCKED;
	}
    for (size_t i = num_pages + 1; i < 1024; ++i) {
		first_page_table[i] = 0;
	}

	/*
	* Set up recursive mapping by mapping the 1024th page table to
	* the page directory. See arch_vas_set_entry for an explaination of why we do this.
    * "Locking" this page directory entry is the only we can lock the final page of virtual
    * memory, due to the recursive nature of this entry.
	*/
	kernel_page_directory[1023] = ((size_t) kernel_page_directory - KERNEL_VIRT_ADDR) | x86_PAGE_PRESENT | x86_PAGE_WRITABLE | x86_PAGE_LOCKED;

	/*
	* Setup the virtual_address_space* structure correctly.
	*/
	spinlock_init(&kernel_vas[cpu_get_count()].lock, "kernel vas lock");

	kernel_vas[cpu_get_count()].data = &kernel_vas_x86[cpu_get_count()];
	kernel_vas_x86[cpu_get_count()].page_dir_phys = (size_t) kernel_page_directory - KERNEL_VIRT_ADDR;
	kernel_vas_x86[cpu_get_count()].page_dir_virt = (size_t) kernel_page_directory;
    
	/*
	* Load the virtual address space.
	*
	* We can't call vas_load(...) as it will try to set current_cpu->current_vas,
	* but current_cpu isn't mapped until later in this function.
	*/
	arch_vas_load(&kernel_vas_x86[cpu_get_count()]);
	
	/* 
	* The virtual memory manager is now initialised, so we can fill in 
	* the rest of the kernel state. This is important as we need all of 
	* the kernel address spaces to share page tables, so we must allocate
	* them all now so new address spaces can copy from us.
	*/
	
	for (int i = 769; i < 1023; ++i) {
        spinlock_acquire(&kernel_vas[cpu_get_count()].lock);
		allocate_page_table(&kernel_vas[cpu_get_count()], kernel_page_directory, i);
        spinlock_release(&kernel_vas[cpu_get_count()].lock);
	}
	
	setup_current_cpu();

	/*
	* Two reasons why we do this:
	*		- we need to flush the TLB
	*		- vas_load may do things that arch_vas_load doesn't do, so it is probably
	*		  important to call vas_load before we do much else
	*/
	vas_load(current_cpu->current_vas);
}

/*
* Creates a new virtual address space with recursive page table mapping, and the kernel mapped.
*/
void arch_vas_create(struct virtual_address_space* vas)
{       
	vas->data = malloc(sizeof(struct x86_vas));

	struct x86_vas* data = (struct x86_vas*) (vas->data);
    data->page_dir_phys = phys_allocate_page();
    data->page_dir_virt = virt_allocate_unbacked_krnl_region(4096);

    /*
    * Map in the page directory.
    * Paging out the page directory would be a catastrophe, so lock it.
    */
    arch_vas_set_entry(vas_get_current_vas(), data->page_dir_virt, data->page_dir_phys, VAS_FLAG_PRESENT | VAS_FLAG_LOCKED | VAS_FLAG_USER | VAS_FLAG_WRITABLE);

	size_t* page_dir_entries = (size_t*) data->page_dir_virt;

	for (int i = 0; i < 768; ++i) {
		page_dir_entries[i] = 0;
	}

	/*
	* Load the same kernel page tables as all other kernel address spaces.
	*/
	for (int i = 768; i < 1023; ++i) {
		page_dir_entries[i] = kernel_page_directory[i];
	}

	/*
	* Set up recursive mapping (see arch_vas_set_entry)
	*/
	page_dir_entries[1023] = data->page_dir_phys | x86_PAGE_PRESENT | x86_PAGE_LOCKED | x86_PAGE_WRITABLE;
}


/*
* Destory an address space. We must free any non-kernel physical pages that
* are still pointed to, and deallocate the memory used to store the address space.
*/
void arch_vas_destroy(void* vas)
{
	// TODO: free physical memory

	free(vas);
}


/*
* Switch to a virtual address space. All future memory accesses will be done
* through this address space until it is changed again.
*/
void arch_vas_load(void* vas_)
{
	struct x86_vas* vas = (struct x86_vas*) vas_;
	x86_set_cr3(vas->page_dir_phys);
}

/*
* Creates a complete copy of an address space and all of the data within it.
*/
void arch_vas_copy(struct virtual_address_space* in, struct virtual_address_space* out)
{
	/*
	* Initialise an address space with the kernel mapped correctly.
	*/ 
	arch_vas_create(out);

	struct x86_vas* in_data  = (struct x86_vas*) (in->data);
	struct x86_vas* out_data = (struct x86_vas*) (out->data);

	size_t* in_page_dir = (size_t*) in_data->page_dir_virt;
	size_t* out_page_dir = (size_t*) out_data->page_dir_virt;

	/*
	* Now we just need to copy the usermode part of the address space.
	* We do this by copying the page tables, and then marking any actual
	* pages as copy on write.
	*/ 
	for (int table_num = 0; table_num < 768; ++table_num) {
		if (in_page_dir[table_num] & x86_PAGE_PRESENT) {
			int flags = in_page_dir[table_num] & 0xFFF;

			size_t new_phys = phys_allocate_page();
			size_t new_virt = virt_allocate_unbacked_krnl_region(4096);
			arch_vas_set_entry(vas_get_current_vas(), new_virt, new_phys, VAS_FLAG_PRESENT | VAS_FLAG_WRITABLE | VAS_FLAG_LOCKED);

			out_page_dir[table_num] = new_phys | flags;

			size_t* old_page_table = (size_t*) (RECURSIVE_MAPPING_ADDR + table_num * PAGE_SIZE);
			size_t* new_page_table = (size_t*) new_virt;

			for (int page_num = 0; page_num < 1024; ++page_num) { 
				size_t old_entry = old_page_table[page_num];

				new_page_table[page_num] = old_entry;

				/*
				* Only mark writable pages as copy on write. Therefore, when we get a fault
				* related to COW, we know that we can set the page back to writable. Read-only
				* pages don't need to be copied anyway. <-- TODO: not sure about that!
				*
				* COW needs to be set in both the old and new page tables.
				*/
				if (old_entry & x86_PAGE_WRITABLE) {
					new_page_table[page_num] |= x86_PAGE_COPY_ON_WRITE;
					new_page_table[page_num] &= ~x86_PAGE_WRITABLE;

					old_page_table[page_num] |= x86_PAGE_COPY_ON_WRITE;
					old_page_table[page_num] &= ~x86_PAGE_WRITABLE;
				}
			}

			arch_vas_set_entry(vas_get_current_vas(), new_virt, 0, 0);

		} else {
			out_page_dir[table_num] = in_page_dir[table_num];
		}
	}
}

/*
* Convert platform-independent flags to the flags actually used by the x86.
*/
static int x86_generic_flags_to_real(int flags)
{
	int out = 0;

	if (flags & VAS_FLAG_WRITABLE)		        out |= x86_PAGE_WRITABLE;
	if (flags & VAS_FLAG_USER)			        out |= x86_PAGE_USER;
	if (flags & VAS_FLAG_PRESENT)		        out |= x86_PAGE_PRESENT;
	if (flags & VAS_FLAG_LOCKED)		        out |= x86_PAGE_LOCKED;
	if (flags & VAS_FLAG_COPY_ON_WRITE)	        out |= x86_PAGE_COPY_ON_WRITE;
	if (flags & VAS_FLAG_ALLOCATE_ON_ACCESS)	out |= x86_PAGE_ALLOCATE_ON_ACCESS;

	return out;
}

static int x86_real_flags_to_generic(int flags) {
    int out = 0;

	if (flags & x86_PAGE_WRITABLE)		        out |= VAS_FLAG_WRITABLE;
	if (flags & x86_PAGE_USER)			        out |= VAS_FLAG_USER;
	if (flags & x86_PAGE_PRESENT)		        out |= VAS_FLAG_PRESENT;
	if (flags & x86_PAGE_LOCKED)		        out |= VAS_FLAG_LOCKED;
	if (flags & x86_PAGE_COPY_ON_WRITE)	        out |= VAS_FLAG_COPY_ON_WRITE;
	if (flags & x86_PAGE_ALLOCATE_ON_ACCESS)	out |= VAS_FLAG_ALLOCATE_ON_ACCESS;

	return out;
}

/*
* TODO: usage of page 1022 is a bit sketchy without a lock to protect it from
*       being changed...
*/
size_t* x86_get_entry(struct virtual_address_space* vas_, size_t virt_addr, bool allow_allocation) {
	struct x86_vas* vas = (struct x86_vas*) vas_->data;

	size_t table_num = virt_addr / 0x400000;
	size_t* page_dir = (size_t*) vas->page_dir_virt;

	/*
	* Not all page tables may have been used yet, and therefore they would not
	* have been allocated. If our page needs a non-existant page table to exist
	* we had better create it before we go any futher.
	*/
	if (!(page_dir[table_num] & x86_PAGE_PRESENT)) {
		if (virt_addr >= KERNEL_VIRT_ADDR) {
			panic("the kernel page tables were meant to already be there!");
		}

		/*
		* Sometimes we don't want to allocate the table if it isn't there, such
		* as when an application just randomly accesses memory that doesn't exist.
		*/
		if (!allow_allocation) {
			return NULL;
		}

		allocate_page_table(vas_, page_dir, table_num);
	}

	size_t page_num = (virt_addr % 0x400000) / PAGE_SIZE;

	/*
	* Use the recursive mapping of the page directory to map the page.
	*
	* What's going on here? On x86, page tables and the page directory
	* share the same structure. Therefore, the page directory can act
	* as a page table. This is why it is valid for it to be the 1024th 
	* page table entry. This us useful as it allows us to access page tables
	* without needing to explicitly map them in. You work this through
	* until you are satisfied that it works, but essentially what happens
	* is that the 1024 page tables pointed to by the page directory are
	* mapped into the last 4MB of RAM.
	*
	* For example, if we wanted to read page table 1023, we would look at the
	* final page of memory. Page table 1022 is mapped in 2nd last page of memory.
	* And thus page 0 is mapped in the 1024th last page of memory.
	*
	* A more in-depth explaination can be found here:
	* 		https://web.archive.org/web/20220422111405/https://wiki.osdev.org/User:Neon/Recursive_Paging
	*
	* This only works for the current address space, but can be adapted to work in
	* non-current address spaces. We can use the page directory address as the
	* 1022th kernel page table. This lets us access the page tables of some other 
	* address space by using the memory at 0xFF800000.
	* 
	* Note that because the kernel page tables are shared between all address
	* spaces, we can treat any pages above KERNEL_VIRT_ADDR to be always 'current'.
	*/
	
	size_t current_vas_base = vas->page_dir_phys;

	size_t* page_table;
	size_t recursive_base_addr = RECURSIVE_MAPPING_ADDR;

	/*
	* Map a page not in the current address space.
	*/
	if (virt_addr < KERNEL_VIRT_ADDR && current_vas_base != vas->page_dir_phys) {
        kprintf("weirdo mapping\n");
        kernel_page_directory[1022] = current_vas_base | x86_PAGE_PRESENT | x86_PAGE_WRITABLE;
		vas_flush_tlb();

		recursive_base_addr = RECURSIVE_MAPPING_ALT_ADDR;
	}

	page_table = (size_t*) (recursive_base_addr + table_num * PAGE_SIZE);
    
    assert(((size_t) page_table) + page_num * sizeof(size_t) >= RECURSIVE_MAPPING_ADDR);
    return page_table + page_num;
}

static void x86_unmark_copy_on_write(struct virtual_address_space* vas, size_t virt_addr) {
	size_t* entry = x86_get_entry(vas, virt_addr, false);
	assert(entry != NULL);
	assert((*entry) & x86_PAGE_COPY_ON_WRITE);

	*entry |= x86_PAGE_WRITABLE;
	*entry &= ~x86_PAGE_COPY_ON_WRITE;

	if (vas->copied_from != NULL) {
		x86_unmark_copy_on_write(vas->copied_from, virt_addr);
	}
}

static void x86_perform_copy_on_write(size_t virt_addr) {
	size_t* entry = x86_get_entry(current_cpu->current_vas, virt_addr, false);
	assert(entry != NULL);
	assert((*entry) & x86_PAGE_COPY_ON_WRITE);
	
	/*
	* Store a copy of the data in the page.
	*/
	uint8_t page_data[PAGE_SIZE];
	memcpy(page_data, (const void*) (virt_addr & ~0xFFF), PAGE_SIZE);

	/*
	* Create a new physical page for the data to go in.
	*/
	size_t new_phys = phys_allocate_page();

	/*
	* Set the virtual page to the new physical page.
	*/
	*entry &= ~0xFFF;
	*entry |= new_phys;
	vas_flush_tlb();

	/*
	* Copy the old data to the new physical page.
	*/
	memcpy((void*) (virt_addr & ~0xFFF), page_data, PAGE_SIZE);

	*entry |= x86_PAGE_WRITABLE;
	*entry &= ~x86_PAGE_COPY_ON_WRITE;

	x86_unmark_copy_on_write(current_cpu->current_vas, virt_addr);
}


extern size_t x86_get_cr2(void);

int x86_handle_page_fault(struct x86_regs* regs) {
    size_t virt_addr = x86_get_cr2();

    spinlock_acquire(&current_cpu->current_vas->lock);

    kprintf("PF (cr2 = 0x%X, eip = 0x%X, err = 0x%X)\n", virt_addr, regs->eip, regs->err_code);

	size_t* entry = x86_get_entry(current_cpu->current_vas, virt_addr, false);

	/*
	* The page table doesn't exist for this address.
	*/
	if (entry == NULL) {
        kprintf("entry == NULL (cr2 = 0x%X, eip = 0x%X, err = 0x%X)\n", virt_addr, regs->eip, regs->err_code);
		return EFAULT;
	}

    if ((*entry & x86_PAGE_ALLOCATE_ON_ACCESS) && !(*entry & x86_PAGE_PRESENT)) {
        size_t page = phys_allocate_page();
        *entry &= ~x86_PAGE_ALLOCATE_ON_ACCESS;
        *entry |= x86_PAGE_PRESENT;
        *entry |= page;
        arch_flush_tlb();
        spinlock_release(&current_cpu->current_vas->lock);
        return 0;
    }

	if ((*entry & x86_PAGE_COPY_ON_WRITE) && (*entry & x86_PAGE_PRESENT)) {
		assert(!(*entry & x86_PAGE_WRITABLE));
	
		x86_perform_copy_on_write(virt_addr);
        spinlock_release(&current_cpu->current_vas->lock);
        return 0;
	} 

    if (*entry & x86_PAGE_LOCKED) {
		kprintf("x86_PAGE_LOCKED (cr2 = 0x%X, eip = 0x%X, err = 0x%X)\n", virt_addr, regs->eip, regs->err_code);
        while (1) {
            ;
        }
        return EFAULT;
    }

    /*
    * Reload the page from the swapfile.
    */
    size_t id = (*entry) >> 12;

    /*
    * Need to release the lock, as phys_allocate_page() may cause a page to be
    * written to the disk.
    */
    spinlock_release(&current_cpu->current_vas->lock);

    size_t phys_page = phys_allocate_page();

    spinlock_acquire(&current_cpu->current_vas->lock);

    arch_vas_set_entry(vas_get_current_vas(), virt_addr & ~0xFFF, phys_page, VAS_FLAG_LOCKED | VAS_FLAG_PRESENT);
    swapfile_read((uint8_t*) (virt_addr & ~0xFFF), id);

    spinlock_release(&current_cpu->current_vas->lock);

    arch_flush_tlb();

	return 0;
}


/*
* Map a page of virtual memory to a physical memory page. This has to be done
* before accessing any virtual memory, otherwise there won't actually be any
* physical memory to 'back it'.
*/
void arch_vas_set_entry(struct virtual_address_space* vas, size_t virt_addr, size_t phys_addr, int flags)
{
	flags = x86_generic_flags_to_real(flags);
    
	size_t* page_entry = x86_get_entry(vas, virt_addr, true);
	assert(page_entry != NULL);
	*page_entry = phys_addr | flags;
}

void arch_vas_get_entry(struct virtual_address_space* vas, size_t virt_addr, size_t* phys_addr_out, int* flags_out) {
	size_t* page_entry = x86_get_entry(vas, virt_addr, true);
	assert(page_entry != NULL);

    *flags_out = x86_real_flags_to_generic(*page_entry & 0xFFF);
    *phys_addr_out = *page_entry & ~0xFFF;
}

size_t arch_find_page_replacement_virt_address(struct virtual_address_space* vas) {
    /*
    * TODO: keep track of the position so next time we don't start from the start again
    */

    for (size_t i = 0x00000000U; i < 0xFF800000U; i += ARCH_PAGE_SIZE) {
        /* 
        * We definitely don't want to allocate page tables if we're already out of memory.
        */
        size_t* page_entry = x86_get_entry(vas, i, false);

        if (page_entry != NULL) {
            if ((*page_entry & x86_PAGE_PRESENT) && !(*page_entry & x86_PAGE_LOCKED)) {
                return i;
            }

        } else {
            /*
            * The page directory there doesn't exist - can skip to the nearest 4MB to save
            * time (this check is to prevent any overflow).
            */
            if (i < 0xFF400000) {
                i = (i + 0x400000) & ~0x3FFFFF;

                /* Counteract the the i += 4096 in the loop */
                i -= ARCH_PAGE_SIZE;
            }
        }
    }

    panic("out of memory");
    return 0;
} 