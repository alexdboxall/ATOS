
#include <common.h>
#include <machine/interrupt.h>
#include <machine/pic.h>
#include <machine/mem/virtual.h>
#include <errno.h>
#include <arch.h>
#include <panic.h>
#include <string.h>
#include <kprintf.h>
#include <errno.h>
#include <syscall.h>
#include <thread.h>

#define PAGE_FAULT_VECTOR   14
#define SYSCALL_VECTOR      96

int (*x86_irq_handlers[256])(struct x86_regs*);


int x86_handle_syscall(struct x86_regs* regs) {
    size_t args[4];
    args[0] = regs->ebx;
    args[1] = regs->ecx;
    args[2] = regs->esi;
    args[3] = regs->edi;

    int res = syscall_perform(regs->eax, args);

    regs->eax = res;
    return res;
}

void x86_interrupt_initialise(void) {
    memset(x86_irq_handlers, 0, sizeof(x86_irq_handlers));

    x86_register_interrupt_handler(PAGE_FAULT_VECTOR, x86_handle_page_fault);
    x86_register_interrupt_handler(SYSCALL_VECTOR, x86_handle_syscall);
}

int x86_register_interrupt_handler(int num, int(*handler)(struct x86_regs*)) {
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

static const char* get_fault_name(int num) {
    if (num < 19) {
        return exception_names[num];
    } else {
        return "unknown";
    }
}

void x86_interrupt_handler(struct x86_regs* r) {
    int num = r->int_no;

    if (pic_is_spurious(num)) {
        return;
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

    /*
    * If there is no handler installed, we want faults to fault, and
    * normal IRQs to be fine.
    */
    int status = num < 32 ? EFAULT : 0;
    
    if (x86_irq_handlers[num]) {
        status = x86_irq_handlers[num](r);
    }

    if (status != 0 && num != SYSCALL_VECTOR) {
        kprintf("unhandled exception - %s (%d)\n", get_fault_name(num), num);
        thread_terminate();
    }
}