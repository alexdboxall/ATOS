
#include <arch.h>
#include <kprintf.h>
#include <cpu.h>
#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <assert.h>
#include <spinlock.h>
#include <machine/beeper.h>
#include <machine/config.h>
#include <machine/console.h>
#include <machine/gdt.h>
#include <machine/cpu.h>
#include <machine/tss.h>
#include <machine/idt.h>
#include <machine/portio.h>
#include <machine/interrupt.h>
#include <machine/pic.h>
#include <machine/pit.h>
#include <machine/floppy.h>
#include <machine/ide.h>
#include <machine/ps2keyboard.h>
#include <machine/mem/virtual.h>

/*
* x86/dev/cpu.c - CPU Initialisation
*
* Functions for initialising a x86 processor.
*/

size_t x86_allow_interrupts = 0;

/*
* Finds all of the devices on the system and adds them to the VFS.
*/
void arch_initialise_devices(void)
{
	x86_beeper_register();

	vga_init();
	ps2_initialise();

	floppy_initialise();
	//ide_initialise();
}

/*
* Prepare the CPU for interrupts to be enabled, and initialise system
* devices required for multitasking.
*
* Also initialise the current_cpu structure for this CPU.
*/
void arch_cpu_initialise_bootstrap(void)
{
	/*
	* Also sets up the current_cpu structure.
	*/
	x86_per_cpu_virt_initialise();

	struct x86_cpu_specific_data* data = malloc(sizeof(struct x86_cpu_specific_data));

	current_cpu->platform_specific_data = data;
	current_cpu->current_thread = NULL;
	
	x86_gdt_initialise();
	x86_idt_initialise();
	x86_tss_initialise();

	x86_interrupt_initialise();
	pic_initialise(); 
	pit_initialise(100);
}

void arch_all_cpus_are_done(void) {
	/*
	* Enable interrupts, and allow spinlocks to enable them too.
	* This flag is global, so it is shared by all CPUs. Therefore,
	* it can only be set once all CPUs are initialised.
	*/ 
	x86_allow_interrupts = 1;
	arch_enable_interrupts();
}

void arch_reboot(void) {
	/*
	* Due to some 'historical reasons', the reset pin in on the keyboard
	* controller (because why not?!).
	*/

	uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
	}
    outb(0x64, 0xFE);
    while (1) {
		arch_stall_processor();
	}
}