
;
;
; x86/lowlevel/misc.s - Miscellaneous Functions
;
; Serveral arch_ functions map directly to assembly instructions, so
; implement them here.
;

global arch_enable_interrupts
global arch_disable_interrupts
global arch_stall_processor
global arch_flush_tlb
global arch_read_timestamp

arch_read_timestamp:
	rdtsc
	ret
	
arch_enable_interrupts:
	sti
	ret
	
arch_disable_interrupts:
	cli
	ret
	
arch_stall_processor:
	hlt
	ret

arch_flush_tlb:
	mov eax, cr3
	mov cr3, eax
	ret