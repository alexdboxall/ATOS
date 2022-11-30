
#include <machine/pic.h>
#include <machine/pit.h>
#include <machine/portio.h>
#include <machine/interrupt.h>
#include <thread.h>
#include <assert.h>
#include <common.h>
#include <kprintf.h>

/*
* x86/dev/pit.c - Programmable Interval Timer
*
* A system timer which can generate an interrupt (on IRQ 0) at regular intervals.
*/

uint64_t pit_hertz = 0;

static void pit_handler(struct x86_regs* r) {
    assert(pit_hertz != 0);
	
	(void) r;

	thread_received_timer_interrupt_bsp(1000000000ULL / pit_hertz);
	thread_received_timer_interrupt();
}

void pit_initialise(int hertz) { 
	pit_hertz = (uint64_t) hertz;

	int divisor = 1193180 / hertz;
	outb(0x43, 0x36);
	outb(0x40, divisor & 0xFF);
	outb(0x40, divisor >> 8);

    x86_register_interrupt_handler(PIC_IRQ_BASE + 0, pit_handler);
}