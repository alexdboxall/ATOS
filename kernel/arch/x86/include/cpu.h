#pragma once

#include <machine/gdt.h>
#include <machine/tss.h>
#include <machine/idt.h>
#include <spinlock.h>

struct x86_cpu_specific_data {
    /* Plz keep tss at the top, thread switching assembly needs it */
    struct tss* tss;

    struct gdt_entry gdt[16];
    struct idt_entry idt[256];
    struct gdt_ptr gdtr;
    struct idt_ptr idtr;
};