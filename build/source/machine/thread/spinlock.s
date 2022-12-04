
;
;
; x86/thread/spinlock.s - Spinlocks
;
; Implement spinlocks in assembly so we can guarentee that they are
; atmoic.
;
;


global arch_irq_spinlock_acquire
global arch_irq_spinlock_release

extern x86_allow_interrupts

x86_spinlocks_held dd 0

arch_irq_spinlock_acquire:
	; We can deadlock if the spinlock is interrupted as it is held,
	; as an interrupt handler may try to lock the same thing
	cli

	; The address of the lock is passed in as an argument
	mov eax, [esp + 4]

.try_acquire:
	; Try to acquire the lock
	lock bts dword [eax], 0
	jc .spin_wait

	; Lock was acquired.
	; Keep track of how many times we have disabled interrupts, so we
	; only enable them again after that many spinlock releases.
	inc dword [x86_spinlocks_held]
	ret

.spin_wait:
	; Lock was not acquired, so do the 'spin' part of spinlock

	; Hint to the CPU that we are spinning
	pause

	; No point trying to acquire it until it is free
	test dword [eax], 1
	jnz .spin_wait
	
	; Now that it is free, we can attempt to atomically acquire it again
	jmp .try_acquire


arch_irq_spinlock_release:
	; The address of the lock is passed in as an argument
	mov eax, [esp + 4]

	dec dword [x86_spinlocks_held]
	jnz .noEnableIRQ
	
	cmp [x86_allow_interrupts], dword 0
	je .noEnableIRQ
	
	sti

.noEnableIRQ:
	lock btr dword [eax], 0
	ret
