#pragma once

/*
* arch.h - Architecture-specific wrappers
*
* 
* Functions relating to hardware devices that must be implemented by
* any platform supporting the operating system.
* 
*/

/*
* config.h needs to define the following:
*	- ARCH_PAGE_SIZE
*	- either ARCH_STACK_GROWS_DOWNWARD or ARCH_STACK_GROWS_UPWARD
*	- ARCH_MAX_CPU_ALLOWED
* 	- the valid user area, via ARCH_USER_AREA_BASE and ARCH_USER_AREA_LIMIT
* 	- the valid kernel area, via ARCH_KRNL_SBRK_BASE and ARCH_KRNL_SBRK_LIMIT
*    		(the kernel and user areas must not overlap, but ARCH_USER_AREA_LIMIT may equal ARCH_KRNL_SBRK_BASE
 			 or ARCH_KRNL_SBRK_LIMIT may equal ARCH_USER_AREA_BASE)
*	- the user stack area, via ARCH_USER_STACK_BASE and ARCH_USER_STACK_LIMIT
*       	(may overlap with ARCH_USER_AREA_BASE and ARCH_USER_AREA_LIMIT)
*/


#include <machine/config.h>


#if ARCH_USER_STACK_BASE < ARCH_USER_AREA_BASE
#error "ARCH_USER_STACK_BASE must be greater than or equal to ARCH_USER_AREA_BASE"
#elif ARCH_USER_STACK_LIMIT > ARCH_USER_AREA_LIMIT
#error "ARCH_USER_STACK_LIMIT must be less than or equal to ARCH_USER_AREA_LIMIT"
#endif

#include <common.h>

struct arch_memory_range
{
	size_t start;
	size_t length;
};

struct virtual_address_space;
struct thread;

struct arch_driver_t;

/*
* Needs to setup any CPU specific structures, set up virtual memory for the system
* and get the current_cpu structure sorted out.
*/
void arch_cpu_initialise_bootstrap(void); 

/*
* Called once after all CPUs are initialised.
*/
void arch_all_cpus_are_done(void);

void arch_initialise_devices_no_fs(void);
void arch_initialise_devices_with_fs(void);

/*
* Only to be called in very specific places, e.g. turning interrupts
* on for the first time, the panic handler.
*/
void arch_enable_interrupts(void);
void arch_disable_interrupts(void);

/*
* Do nothing until (maybe) the next interrupt. If this is not supported by the
* system it may just return without doing anything.
*/
void arch_stall_processor(void);

void arch_reboot(void);

void arch_irq_spinlock_acquire(volatile size_t* lock);
void arch_irq_spinlock_release(volatile size_t* lock);

/*
* Guaranteed to be called with sequential indexes from 0. No index will be
* repeated or skipped. No locking is required, and an address of a static
* local object is permitted to be returned.
* 
* NULL is returned if there is no more memory. No more calls to this function
* will be made after a NULL is returned.
*/
struct arch_memory_range* arch_get_memory(int index) warn_unused;

void arch_flush_tlb(void);

/*
* Needs only to set the 'data' field of the struct virtual_address_space*
*/
void arch_vas_create(struct virtual_address_space* vas);
void arch_vas_copy(struct virtual_address_space* in, struct virtual_address_space* out);


void arch_vas_destroy(void* vas);
void arch_vas_load(void* vas);
void arch_vas_set_entry(struct virtual_address_space* vas_, size_t virt_addr, size_t phys_addr, int flags);
void arch_vas_get_entry(struct virtual_address_space* vas_, size_t virt_addr, size_t* phys_addr_out, int* flags_out);

int arch_exec(void* data, size_t data_size, size_t* entry_point, size_t* sbrk_point);

void arch_set_forked_kernel_stack(struct thread* original, struct thread* forked);

/*
* Only called the first time a given thread switches into usermode
*/
void arch_switch_to_usermode(size_t address, size_t stack);

/* 
* Must set current_cpu->current_thread correctly. 
* (As well as saving/switching/reloading threads of course.)
*/
void arch_switch_thread(struct thread* thread);

size_t arch_prepare_stack(size_t addr);

uint64_t arch_read_timestamp(void);

size_t arch_load_driver(void* data, size_t data_size, size_t relocation_point);
int arch_start_driver(size_t driver, void* argument);

size_t arch_find_page_replacement_virt_address(struct virtual_address_space* vas);