
#include <heap.h>
#include <machine/tss.h>
#include <machine/cpu.h>
#include <machine/gdt.h>
#include <cpu.h>
#include <kprintf.h>

extern void x86_tss_load(size_t selector);

void x86_tss_initialise(void) {
    struct x86_cpu_specific_data* cpu_data = current_cpu->platform_specific_data;
    cpu_data->tss = (struct tss*) malloc(sizeof(struct tss));

    cpu_data->tss->link = 0x10;
    cpu_data->tss->esp0 = 0;
    cpu_data->tss->ss0 = 0x10;
    cpu_data->tss->iopb = sizeof(struct tss);

    uint16_t selector = x86_gdt_add_tss(cpu_data->tss);
    x86_tss_load(selector);
}