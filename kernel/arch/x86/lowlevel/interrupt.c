
#include <common.h>
#include <machine/interrupt.h>
#include <machine/pic.h>
#include <errno.h>
#include <panic.h>
#include <string.h>
#include <kprintf.h>

void (*x86_irq_handlers[256])(struct x86_regs*);

void x86_interrupt_initialise(void) {
    memset(x86_irq_handlers, 0, sizeof(x86_irq_handlers));
}

int x86_register_interrupt_handler(int num, void(*handler)(struct x86_regs*)) {
    if (x86_irq_handlers[num] != NULL) {
        return EALREADY;
    }

    if (num < 0 || num >= 256) {
        return EINVAL;
    }

    x86_irq_handlers[num] = handler;
    return 0;
}

static char exception_names[19][32] = {
    "division by zero",
    "debug interrupt",
    "non-maskable interrupt",
    "breakpoint interrupt",
    "overflow exception",
    "bound range exceeded",
    "invalid opcode fault",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid task state segment",
    "segment not present",
    "stack segment fault",
    "general protection fault",
    "page fault",
    "other fault",
    "coprocessor fault",
    "alignment check fault",
    "machine check fault",
};

void x86_interrupt_handler(struct x86_regs* r) {
    int num = r->int_no;

    if (pic_is_spurious(num)) {
        return;
    }

    if (num < 32) {
        if (num < 19) {
            kprintf("eip = 0x%X, err = 0x%X\n", r->eip, r->err_code);
            panic(exception_names[num]);
        } else {
            panic("an unknown exception occured");
        }
    }

    /*
    * We need to acknowledge the interrupt before we potentially switch
    * threads during the handler.
    * 
    * TODO: ?? have a nested postpone counter for IRQs, when it hits zero,
    * perform postponed switches. see https://forum.osdev.org/viewtopic.php?f=1&t=26617
    * 
    */
    if (num >= PIC_IRQ_BASE && num < PIC_IRQ_BASE + 16) {
        pic_eoi(num);
    }
    
    if (x86_irq_handlers[num]) {
        x86_irq_handlers[num](r);
    }
}