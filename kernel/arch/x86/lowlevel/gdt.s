
;
;
; x86/lowlevel/gdt.s - Load GDT
;
; We need assembly in order to access the LGDT instruction so we can load
; the GDT. Hence we define a function in assembly to load the GDT for us.
;
;


global x86_gdt_load
x86_gdt_load:
	; The address of the GDTR is passed in as an argument
	mov eax, [esp + 4]
	lgdt [eax]

	; We now need to reload CS using a far jump...
	jmp 0x08:.reloadSegments

.reloadSegments:
	; And all of the other segments by loading them
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	ret