
;
;
; x86/mem/virtual.asm - Virtual Address Space Loading
;
; We load a virtual address space by putting the physical address of the 
; page directory in the CR3 control register.
;
;

global x86_set_cr3

x86_set_cr3:
	mov eax, [esp + 4]
	mov cr3, eax
	ret
