
; x86 Bootloader
;
; This is the bootloader for the kernel - the very first thing that is run after
; the firmware finishes its initialization. The BIOS loads the first sector
; (512 bytes) from the disk into memory at address 0x7C00, and jumps to it.
;
; That 512 byte program is responsible for booting the entire kernel. On normal systems,
; it tends to load a second bootloader which can be of any size, which then loads the
; kernel.
;
; However, as we are using DemoFS, it is very easy for us to load the kernel, as it
; stores the size and sector number of the kernel, as well as reserving the first 8
; sectors for additional bootloader code. Hence our bootloader can be 8 sectors (4KBs)
; long (when assembled). However, as the BIOS only loads the first sector, that first
; sector needs to load the next 7 into memory.
;
; Below is an explaination of some relevant concepts:
;
;					*************** THE BIOS ***************
;
; The BIOS (Basic Input/Output System) is the firmware stored in ROM, and is *really* the first
; thing that runs. It is responsible for initializing the hardware (e.g. the memory controllers),
; testing the CPU, setting up the absolute basics in terms of hardware (i.e. a terminal, keyboard,
; and disk), and then jumping to the bootloader.
;
; The BIOS sets up interrupt handlers so the bootloader can also make use of the hardware drivers
; the BIOS contains. For example, by calling INT 0x10, the BIOS will perform video functions, and
; INT 0x13 will access the disk. The other registers contain arguments and return values. There is
; no official standard for this, but certain interrupts are 'standard enough' for us to use.
;
; We must use the BIOS, as we don't have enough room in our bootloader to a disk driver, or a driver
; for every known video card in existence. 
;
;
; 					*************** REAL MODE ***************
;
; Note that when the computer starts, we are in real mode (16 bit mode). Hence we need
; to deal with 16 bit segments (see below), and thus we can only have 20 bit memory addresses
; (i.e. up to 1MB of memory). There is also a lot of stuff 'lurking' in this first megabyte
; of memory, such as BIOS data, interrupt tables, etc. that we shouldn't touch.
;
; You'll notice that 32 bit registers are still accessible in 16 bit mode. This only works on 
; processors that actually do have a 32 bit mode (i.e. an 80386 or above), but as our kernel
; is for 32 bit processors, we can assume we have 32 bit mode.
;
; In 16 bit mode, memory accesses consist of two parts, a 16 bit offset (the address),
; and a 16 bit segment. They are combined as follows:
;
;		absolute address = segment * 16 + offset
;
; Addresses are written like this:
;		0x1234:0xABCD
;		 ^ 		   ^
;		segment    offset
;
; Note that there are many ways of representing the same absolute memory address. For example:
;
;		0x0123:0x0004, 0x0000:0x1234, and 0x0077:0x0AC4
;
; All represent the same absolute address of 0x01234.
;
; You normally only need to specify the offset when making memory accesses. Normal memory
; reads and writes will use the segment stored in the DS register. Code access always uses
; the CS register. The stack uses SS, and there are additional registers ES, FS and GS which
; you can use if you need to specific an explicit segment.
;
; Note that there are some other 'gotchas' in 16 bit mode and with segments, such as:
;		- certain instructions having default segment register overrides (e.g. movsb)
;		- absolute addresses can sometimes go above 1MB(!) (e.g. 0xFFFF:0x0010 -> 0x100000)
;		  (look up the A20 gate if you're interested in this)
;
;
; 				*************** OVERALL BOOT PROCESS ***************
;
;		- BIOS loads the first sector of our bootloader to 0x7C00 and runs it
;		- Our bootloader initialises segment registers, stack, flags, etc.
;		- Our bootloader loads the remaining seven sectors of our bootloader into memory
;		- Our bootloader loads the kernel file to address 0x10000 (64KB)
;		- Our bootloader enables the A20 gate
;		- Our bootloader gets a memory map from the BIOS, which will later be passed to the kernel
;		- Our bootloader sets up a GDT, enables and then switches to protected mode (32 bit mode)
;		- Our bootloader parses the kernel file, and loads the required sections into memory at 0x100000 (1MB)
;		- Our bootloader jumps to the kernel, passing it the memory map
;		- The ATOS kernel runs!		
;

org 0x7C00
bits 16

jmp short start
nop

kernel_kilobytes dw 0x0
boot_drive_number db 0

start:
	; This bootloader is at 0x7C00, but we don't know which combination of segment
	; and offset. This bootloader is meant to be loaded at 0x0000:0x7C00, so do a 
	; far jump to segment 0x0000.
	cli
	cld
	jmp 0:set_cs

set_cs:
	; Zero out the other segments. This allows for correct access of data.
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; Set the stack to 0x0000:0x0000, so it will wrap around to
	; 0x0000:0xFFFE and so on.
	mov ss, ax
	mov sp, ax

	; The BIOS puts the 'boot drive number' in DL. This can be passed to BIOS 
	; disk calls to select the correct disk.
	mov [boot_drive_number], dl

	; Clear the screen by calling the BIOS.
	mov ax, 0x3
	int 0x10
	
	; Load the rest of the bootloader, and the DemoFS table.
	xor eax, eax
	inc ax
	mov cx, 8
	mov di, 0x7C00 + 512
	call read_sector

	; Read the kernel's inode number, and size.
	mov eax, [0x7C00 + 512 * 8 + 12]
	mov ecx, eax
	
	; Inode number
	and eax, 0xFFFFFF
	
	; Size in kilobytes (must do two shifts to chop off the low bit,
	; or I guess you could do shr ebx, 23; and bl, 0xFE)
	shr ecx, 24
	shl ecx, 1
	mov [kernel_kilobytes], cx
	
	; Load a sector at at time to 0x1000:0x0000
	; ie. 0x10000. This allows sizes of approx. 448KB (our filesystem
	; only supports a kernel size of 255KB anyway).
	mov bx, 0x1000
	mov gs, bx
	xor di, di	

next_sector:
	push cx
	mov cx, 1
	call read_sector
	pop cx

	; Move to next sector
	inc eax
	
	; Move 512 bytes along ( = 0x200 bytes = offset change of 0x20)
	mov bx, gs
	add bx, 0x20
	mov gs, bx
	loop next_sector

	call generate_memory_map


    ; Get video mode information
    mov ax, 0x7000 
    mov es, ax
    mov ax, 0x4F01
    mov cx, 0x4118
    xor di, di
    int 0x10

    ; Set video mode
    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10

	; Jump to 32 bit mode.
	lgdt [gdtr]
	mov eax, cr0
	or al, 1
	mov cr0, eax
	jmp 0x08:start_protected_mode


; The number of entries will be stored at 0x500, the map will be put at 0x504
; From here:
;		https://wiki.osdev.org/Detecting_Memory_(x86)
generate_memory_map:
    mov di, 0x504          ; Set di to 0x8004. Otherwise this code will get stuck in `int 0x15` after some entries are fetched 
	xor ebx, ebx		; ebx must be 0 to start
	xor bp, bp		; keep an entry count in bp
	mov edx, 0x0534D4150	; Place "SMAP" into edx
	mov eax, 0xe820
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes
	int 0x15
	;jc short .failed	; carry set on first call means "unsupported function"
	;mov edx, 0x0534D4150	; Some BIOSes apparently trash this register?
	;cmp eax, edx		; on success, eax must have been reset to "SMAP"
	;jne short .failed
	;test ebx, ebx		; ebx = 0 implies list is only 1 entry long (worthless)
	;je short .failed
	jmp short .jmpin
.e820lp:
	mov eax, 0xe820		; eax, ecx get trashed on every int 0x15 call
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes again
	int 0x15
	jc short .e820f		; carry set means "end of list already reached"
	mov edx, 0x0534D4150	; repair potentially trashed register
.jmpin:
	jcxz .skipent		; skip any 0 length entries
	cmp cl, 20		; got a 24 byte ACPI 3.X response?
	jbe short .notext
	test byte [es:di + 20], 1	; if so: is the "ignore this data" bit clear?
	je short .skipent
.notext:
	mov ecx, [es:di + 8]	; get lower uint32_t of memory region length
	or ecx, [es:di + 12]	; "or" it with upper uint32_t to test for zero
	jz .skipent		; if length uint64_t is 0, skip entry
	inc bp			; got a good entry: ++count, move to next storage spot
	add di, 24
.skipent:
	test ebx, ebx		; if ebx resets to 0, list is complete
	jne short .e820lp
.e820f:
	mov [0x500], bp	; store the entry count
.failed:
	ret
	

read_sector:
	; Put sector number in EAX. Put the number of sectors in CX.
	; Put the destination offset in DI. The segment will be GS.

	pushad
	push es

	; This attempts to read from an 'extended' hard drive (i.e. all
	; modern drives)
	mov [io_lba], eax
	mov ax, gs
	mov [io_segment], ax
	mov dl, byte [boot_drive_number]
	mov ah, 0x42
	mov si, disk_io_packet
	mov [io_offset], di
	mov [io_count], cx
	int 0x13
	
	; If succeeded, we can exit.
	jnc skip_floppy_read
	
	; Otherwise we must call the older CHS method of reading (likely
	; from a floppy).

	mov ah, 0x8
	xor di, di			;guard against BIOS bugs
	mov es, di
	mov dl, byte [boot_drive_number]
	int 0x13
	jc short $

	inc dh				;BIOS returns one less than actual value

	and cx, 0x3F		;NUM SECTORS PER CYLINDER IN CX

	mov bl, dh			
	xor bh, bh			;NUM HEADS IN BX
	
	lfs ax, [io_lba]	;first load [d_lba] into GS:AX
	mov dx, fs			;then copy GS to DX to make it DX:AX

	div cx

	inc dl
	mov cl, dl

	xor dx, dx
	div bx

	;LBA					0x2000
	;ABSOLUTE SECTOR:	CL	0x03
	;ABSOLUTE HEAD:		DL	0x0A
	;ABSOLUTE CYLINDER:	AX	0x08

	;get the low two bits of AH into the top 2 bits of CL
	and ah, 3
	shl ah, 6
	or cl, ah
						;SECTOR ALREADY IN CL
	mov ch, al			;CYL
	mov ax, [io_count]	;SECTOR COUNT
	mov ah, 0x02		;FUNCTION NUMBER
	mov dh, dl			;HEAD
	mov dl, [boot_drive_number]
	mov bx, [io_segment]
	mov es, bx
	mov bx, [io_offset]
	int 0x13
	jc short $

skip_floppy_read:
	pop es
	popad

	ret



; The GDT. We are required to have this setup before we go into 32 bit mode, as
; it defines the segments we use in 32 bit mode.
align 8
gdt_start:
	dd 0
	dd 0
	
	dw 0xFFFF
	dw 0x0000
	db 0
	db 10011010b
	db 11001111b
	db 0

	dw 0xFFFF
	dw 0x0000
	db 0
	db 10010010b
	db 11001111b
	db 0
gdt_end:

gdtr:
	dw gdt_end - gdt_start - 1
	dd gdt_start


; A data packet we use to interface with the BIOS extended disk functions.
align 16
disk_io_packet:
	db 0x10
	db 0x00
io_count:
	dw 0x0001
io_offset:
	dw 0x0000
io_segment:
	dw 0x0000
io_lba:
	dd 0
	dd 0
	
times 0x1BE - ($-$$) db 0

; Partition table
db 0x80					; bootable
db 0x01
db 0x01
db 0x01
db 0x0C					; pretend to be FAT32
db 0x01
db 0x01
db 0x01
dd 1					; start sector (we put a dummy VBR here)
dd 131072 * 16			; total sectors in partition

times 16 * 3 db 0x00


dw 0xAA55

; Pretend to be a FAT32 partition by having a 'valid enough' VBR
; (this will never get used, it just might 'help' with the USB)

bits 16
jmp short vbr_dummy
nop
db "DUMMYVBR"
dw 0x200
db 0x1
dw 0x187E
db 0x2
dw 0x0
dw 0x0
db 0xF8
dw 0x0
dw 0x0
dw 0x0
dd 0x1
dd 131072
dd 0x3C1
dw 0
dw 0
dd 2
dw 1
dw 6
db "DUMMY DUMMY "
db 0x80
db 0x00
db 0x28
dd 0xDEADC0DE

vbr_dummy:
	jmp 0x7C00

times (512 * 2 - 2) - ($-$$) db 0x90
dw 0xAA55





;
; BEGIN 32 BIT CODE
; 
 
bits 32

start_protected_mode:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov esp, 0x80000
	
	mov eax, 0xB8000	
	mov [eax     ], word 0x0700 | 'B'
	mov [eax +  2], word 0x0700 | 'o'
	mov [eax +  4], word 0x0700 | 'o'
	mov [eax +  6], word 0x0700 | 't'
	mov [eax +  8], word 0x0700 | 'i'
	mov [eax + 10], word 0x0700 | 'n'
	mov [eax + 12], word 0x0700 | 'g'
	mov [eax + 14], word 0x0700 | '.'
	mov [eax + 16], word 0x0700 | '.'
	mov [eax + 18], word 0x0700 | '.'
	
	call enable_a20
	call create_multiboot_tables
	call load_elf
		
	; Point the kernel to the multiboot table
	mov ebx, 0x7E000

	mov eax, [0x10000 + 24]
	push eax
	ret


; From https://wiki.osdev.org/index.php?title=A20_Line
enable_a20:
    call    a20wait
    mov     al,0xAD
    out     0x64,al
 
    call    a20wait
    mov     al,0xD0
    out     0x64,al
 
    call    a20wait2
    in      al,0x60
    push    eax
 
    call    a20wait
    mov     al,0xD1
    out     0x64,al
 
    call    a20wait
    pop     eax
    or      al,2
    out     0x60,al
 
    call    a20wait
    mov     al,0xAE
    out     0x64,al
 
    call    a20wait
    ret
 
a20wait:
    in      al,0x64
    test    al,2
    jnz     a20wait
    ret
 
a20wait2:
    in      al,0x64
    test    al,1
    jz      a20wait2
    ret
	

load_elf:
	mov eax, 0x10000		; elf

	mov ebx, [eax + 28]
	add ebx, 0x10000		; elf->phOffset

	mov cx, [eax + 44]		; elf->phNum

.next_program_header:
	; ebx has the current program header in it
	; cx controls the loop

	mov eax, [ebx + 8]
	mov esi, [ebx + 4]
	mov edi, [ebx + 16]
	mov edx, [ebx + 0]

	; eax has addr
	; esi has file offset
	; edi has size
	; edx has type

	cmp edx, 1
	jne .skip

	; Now we have a loadable segment.

	add esi, 0x10000

	; eax has addr
	; esi has file offset in memory
	; edi has size

	mov edx, [ebx + 20]
	sub edx, edi
	
	; edx has additional null bytes required

	pushad
	cld
	; MEMCPY(EAX - (0xC00[0/1??]0000?) + 0x100000, ESI, EDI)
	mov ecx, edi
	mov edi, eax
	and edi, 0x0FFFFFFF

    mov eax, edi
    call write_hex

    mov eax, esi
    call write_hex

    mov eax, ecx
    call write_hex

    mov eax, edx
    call write_hex

	rep movsb
	popad

	pushad
	mov ecx, edx
	add edi, eax        ;add addr and size together to get where the zeros should start
	and edi, 0x0FFFFFFF
    mov al, 0
	rep stosb
	popad

.skip:
	; Move to next program header
	add ebx, 32
	loop .next_program_header

	ret

create_multiboot_tables:
	mov [0x7E000], dword 1 << 6
	
	xor ebx, ebx
	mov bx, [0x500]
	
	; EBX has the number of BIOS memory entries
	; Need to multiply it by 24 to get the total size of the table.
	
	mov ecx, ebx		; EBX
	shl ebx, 1			; EBX * 2
	add ebx, ecx		; EBX * 3
	shl ebx, 3			; EBX * 24
	
	mov [0x7E000 + 44], ebx
	mov [0x7E000 + 48], dword 0x7F000

	mov edi, 0x7F000
	mov esi, 0x504
	xor ecx, ecx
	mov cx, [0x500]

.next_entry:
; First uint64_t = Base address
;Second uint64_t = Length of "region" (if this value is 0, ignore the entry)
;Next uint32_t = Region "type" 

	; Multiboot entry size (not including this field)
	mov [edi + 0], dword 20

	; Address low
	mov eax, [esi + 0] 
	mov [edi + 4], eax

	; Address high
	mov eax, [esi + 4]
	mov [edi + 8], eax
	
	; Length low
	mov eax, [esi + 8]
	mov [edi + 12], eax

	; Length high
	mov eax, [esi + 12]
	mov [edi + 16], eax

	; Type
	mov eax, [esi + 16]
	cmp eax, 1
	je short .good
	mov [edi + 20], dword 2
	je short .type_done
.good:
	mov [edi + 20], dword 1
.type_done:

	add edi, 24
	add esi, 24
	loop .next_entry

	ret


write_hex:
    pushad

    mov esi, eax
    mov ebx, hex_chars

    mov edi, [cur_y]
    add edi, 0xB8000

    mov ecx, 8
.restart:
    rol esi, 4
    mov eax, esi
    and al, 0xF
    xlat

    mov [edi], al
    inc edi
    mov [edi], byte 0x07
    inc edi
    
    loop .restart

    add [cur_y], dword 40
    popad
    ret

cur_y dd 0
hex_chars db "0123456789ABCDEF"

times (512 * 8) - ($-$$) db 0