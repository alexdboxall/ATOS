
#include <panic.h>
#include <arch.h>
#include <kprintf.h>
#include <assert.h>
#include <console.h>

_Noreturn void panic(const char* message)
{
	kprintf("\n\n *** KERNEL PANIC ***\n\n%s\n", message);

    //console_panic(message);

	while (1) {
		arch_disable_interrupts();
		arch_stall_processor();
	}
}