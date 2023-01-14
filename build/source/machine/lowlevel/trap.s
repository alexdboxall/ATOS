
;
;
; x86/lowlevel/trap.s - Interrupt Trap Handlers
;
; These functions are called when an interrupt occurs. For certain interrupts,
; the CPU pushes an error code. For consistency, we will push our own dummy
; error code for the interrupts where an error code in not automatically pushed.
; We will then push the interrupt number, and then jump to a common handler.
;

global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr96
global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15


; We don't need to disable interrupts - they are automatically disabled when
; the interrupt comes in

isr0:
    push 0
    push 0
    jmp int_common_handler

isr1:
    push byte 0
    push byte 1
    jmp int_common_handler

isr2:
    push byte 0
    push byte 2
    jmp int_common_handler

isr3:
    push byte 0
    push byte 3
    jmp int_common_handler

isr4:
    push byte 0
    push byte 4
    jmp int_common_handler

isr5:
    push byte 0
    push byte 5
    jmp int_common_handler

isr6:
    push byte 0
    push byte 6
    jmp int_common_handler

isr7:
    push byte 0
    push byte 7
    jmp int_common_handler

isr8:
    push byte 8
    jmp int_common_handler

isr9:
    push byte 0
    push byte 9
    jmp int_common_handler

isr10:
    push byte 10
    jmp int_common_handler

isr11:
    push byte 11
    jmp int_common_handler

isr12:
    push byte 12
    jmp int_common_handler

isr13:
    push byte 13
    jmp int_common_handler

isr14:
    push byte 14
    jmp int_common_handler

isr15:
    push byte 0
    push byte 15
    jmp int_common_handler

isr16:
    push byte 0
    push byte 16
    jmp int_common_handler

isr17:
    push byte 17
    jmp int_common_handler

isr18:
    push byte 0
    push byte 18
    jmp int_common_handler

isr19:
    push byte 0
    push byte 19
    jmp int_common_handler
	
isr20:
    push byte 0
    push byte 20
    jmp int_common_handler
	
isr21:
    push byte 0
    push byte 21
    jmp int_common_handler


; This is our system call handler
isr96:
    push byte 0
    push 96
    jmp int_common_handler



; Note that in the PIC setup, we remap our IRQs so they start at 32
; That is, IRQ0 is actually ISR32, etc. up to IRQ15 which is ISR47
; This is so they don't clash with the exceptions above, which are
; not re-mappable.
irq0:
    push byte 0
    push byte 32
    jmp int_common_handler

irq1:
    push byte 0
    push byte 33
    jmp int_common_handler

irq2:
    push byte 0
    push byte 34
    jmp int_common_handler

irq3:
    push byte 0
    push byte 35
    jmp int_common_handler

irq4:
    push byte 0
    push byte 36
    jmp int_common_handler

irq5:
    push byte 0
    push byte 37
    jmp int_common_handler

irq6:
    push byte 0
    push byte 38
    jmp int_common_handler

irq7:
    push byte 0
    push byte 39
    jmp int_common_handler

irq8:
    push byte 0
    push byte 40
    jmp int_common_handler

irq9:
    push byte 0
    push byte 41
    jmp int_common_handler

irq10:
    push byte 0
    push byte 42
    jmp int_common_handler

irq11:
    push byte 0
    push byte 43
    jmp int_common_handler

irq12:
    push byte 0
    push byte 44
    jmp int_common_handler

irq13:
    push byte 0
    push byte 45
    jmp int_common_handler

irq14:
    push byte 0
    push byte 46
    jmp int_common_handler

irq15:
    push byte 0
    push byte 47
    jmp int_common_handler
	

; Our common interrupt handler
extern x86_interrupt_handler
int_common_handler:
    ; Save the registers and segments
    pushad
	push ds
    push es
    push fs
    push gs
	
    ; Ensure we have kernel segments and not user segments
    mov ax, 0x10
	mov ds, ax
    mov es, ax
	mov fs, ax
    mov gs, ax

    ; Allow nested interrupts
    sti
	
    ; Push a pointer to the registers to the kernel handler
    push esp
	cld
    call x86_interrupt_handler
    add esp, 4
	
    ; Restore registers
    pop gs
    pop fs
    pop es
    pop ds
    popad

    ; Skip over the error code and interrupt number (we can't pop them
    ; anywhere, as the registers have already been restored)
    add esp, 8

    ; Return from the interrupt - also restores the stack and the flags
    iretd