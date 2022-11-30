
;
;
; x86/thread/switch.s - Switching Threads
;
;
;


global arch_switch_thread
global arch_prepare_stack

extern thread_startup_handler
extern current_cpu

arch_prepare_stack:
	; We need to put 5 things on the stack - dummy values for EBX, ESI,
	; EDI and EBP, as well as the address of thread_startup_handler
	; 
	; This is because these get popped off in arch_switch_thread

	; Grab the address of the new thread's stack from our stack (it was
	; passed in as an argument)
	mov eax, [esp + 4]

	; We need to get to the bottom position, and we also need to return that
	; address in EAX so it can be put into the struct, so it makes sense to modify it.
	sub eax, 20

	; This is where the address of arch_switch_thread needs to go.
	; +0 is where EBP is, +4 is EDI, +8 for ESI, +12 for EBX,
	; and so +16 for the return value.
	; (see the start of arch_switch_thread for where these get pushed)
	mov [eax + 16], dword thread_startup_handler

	ret

arch_switch_thread:		
	; The new thread is passed in on the stack as an argument.

	; The calling convention we use already saves EAX, ECX and EDX whenever a
	; function is called. Therefore, we only need to save the other four.
	push ebx
	push esi
	push edi
	push ebp

	; We are now free to trash the general purpose registers (except ESP),
	; so we can now load the current task using the current_cpu structure.
	
	; Find the actual address of the current_cpu structure. (current_cpu stores
	; a pointer, it isn't the actual structure).
	mov edi, [current_cpu]

	; The first entry in current_cpu is guaranteed to be the current thread's
	; structure.
	mov edi, [edi + 0]

	; The third entry in a thread structure is guaranteed to be the stack pointer.
	; Save our stack there.
	mov [edi + 8], esp

	; Now we load the new thread's state.

	; The new thread was passed in as an argument on the stack, and we just
	; pushed 4 extra things to the stack. Hence, look back 5 positions to find
	; the new thread's structure.
	mov esi, [esp + (4 + 1) * 4]
	
	; Update the currently running thread in the current_cpu structure.
	mov edi, [current_cpu]
	mov [edi + 0], esi

	; Reload the new thread stack
	mov esp, [esi + 8]

	; The top of the kernel stack (which needs to go in the TSS for
	; user to kernel switches), is the second entry in the thread struct.
	mov ebx, [esi + 4]
	
	; The third entry in current_cpu is a pointer to CPU specific data.
	mov ecx, [edi + 8]

	; The first entry in the CPU specific data is the TSS pointer
	mov ecx, [ecx + 0]

	; Load the TSS's ESP0 with the new thread's stack
	mov [ecx + 4], ebx

	; Now we have the new thread's stack, we can just pop off the state
	; that would have been pushed when it was switched out.
	pop ebp
	pop edi
	pop esi
	pop ebx

	ret