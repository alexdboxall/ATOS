
#include <panic.h>
#include <arch.h>
#include <kprintf.h>
#include <assert.h>

_Noreturn void panic(const char* message)
{
	kprintf("Kernel panic: %s\n", message);

	while (1) {
		arch_disable_interrupts();
		arch_stall_processor();
	}
}