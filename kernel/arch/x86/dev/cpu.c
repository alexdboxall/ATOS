
#include <arch.h>
#include <kprintf.h>
#include <cpu.h>
#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <assert.h>
#include <spinlock.h>
#include <loader.h>
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
* Finds the devices on the system and adds them to the VFS.
* The _no_fs function runs first, and doesn't have access to the filesystem
* (and thus cannot load drivers from the disk). It *must* load a disk driver.
* Everything else can go afterward (in the future PCI would probably go
* in here too).
*/
void arch_initialise_devices_no_fs(void)
{    
	//floppy_initialise();
	ide_initialise();
}

/*
* Finds the devices on the system and adds them to the VFS.
* This runs after _no_fs, and thus has access to the filesystem.
*/
void arch_initialise_devices_with_fs(void) {
    x86_beeper_register();

    /*
    * Don't swap out the VGA console driver, as the page fault
    * handler might use it (to kprintf() or panic())
    */
    load_driver("sys:/vesacon.sys", true);

    load_driver("sys:/ps2.sys", false);

    //load_driver("sys:/ACPICA.SYS", false);
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
	pit_initialise(500);
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