
;
;
; x86/kernel_entry.asm - Kernel Entry Point
;
; We want the kernel to conform to the Multiboot 2 standard, by doing this
; the kernel can be loaded by common bootloaders, such as GRUB2, or directly
; by the QEMU emulator
;
;

; Therefore, we must first define a few constants and flags required by Multiboot
;
MBALIGN		equ 1 << 0
MEMINFO		equ 1 << 1
FLAGS		equ MBALIGN | MEMINFO
MAGIC		equ 0x1BADB002
CHECKSUM	equ -(MAGIC + FLAGS)

; We put the multiboot in a special section which gets placed at the start of
; the binary. This allows the bootloader to find the Multiboot header.
;
section .multiboot.data
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

	
; We need a stack, so for the bootstrap process. We can define a quick 16KB 
; stack in the BSS section, which is always initialised to zeros
;
section .bss
align 16
stack_bottom:
resb 16 * 1024
stack_top:


; The kernel is being loaded to 0xC0100000. We need temporary bootstrap paging
; structures handled here so that we can get the kernel to 0xC0100000 in virtual
; memory.
;
; We will allocate one page directory, and one page table. With this, we can map
; 4MB of memory. As the kernel starts at 1MB, we can actually have a kernel
; of at most 3MB. We will replace these paging structures later once we get into 
; the proper kernel.
;
align 4096
boot_page_directory: resb 4096
boot_page_table1: resb 4096


; The start of the kernel itself - this will be called by the bootloader
; We must place it in a special section so it appears at the start of the binary
;
section .multiboot.text
global _start:function (_start.end - _start)	; calculate size of the _start function
extern kernel_main
_start:
	cli
	cld	

	; Get ready to loop over the page table
	mov edi, boot_page_table1 - 0xC0000000
	xor esi, esi
	mov ecx, 1024

.mapNextPage:
	; Combine the address with the present and writable flags
	mov edx, esi
	or edx, 3
	mov [edi], edx

.incrementPage:
	; Move onto the next page table entry, and the next corresponding physical page
	add esi, 4096
	add edi, 4
	loop .mapNextPage

.endMapping:
	; Identity map and put the mappings at 0xC0000000
	; This way we won't page fault before we jump over to the kernel in high memory
	; (we are still in low memory)
	mov [boot_page_directory - 0xC0000000 + 0], dword boot_page_table1 - 0xC0000000 + 3
	mov [boot_page_directory - 0xC0000000 + 768 * 4], dword boot_page_table1 - 0xC0000000 + 3

	; Set the page directory
	mov ecx, boot_page_directory - 0xC0000000
	mov cr3, ecx

	; Enable paging
	mov ecx, cr0
	or ecx, (1 << 31)
	mov cr0, ecx

	; This is why identity paging was required earlier, as paging is on, but we
	; are still in low memory (i.e. at 0x100000-ish)

	; Now jump to the higher half
	lea ecx, kernel_entry_point
	jmp ecx
.end:

section .text

global x86_grub_table
x86_grub_table dd 0

; The proper entry point of the kernel. Assumes the kernel is mapped into memory
; at 0xC0100000.
kernel_entry_point:
	; Remove the identity paging and flush the TLB so the changes take effect
	mov [boot_page_directory], dword 0
	mov ecx, cr3
	mov cr3, ecx

	; GRUB puts the address of a table in EBX, which we must use to find the
	; memory table. Note that we haven't trashed EBX up until this point.
	mov [x86_grub_table], ebx
	
	; Set the stack to the one we defined
	mov esp, stack_top

	; Jump to the kernel main function
	call kernel_main

	; We should never get here, but halt just in case
	cli
	hlt
	jmp $
