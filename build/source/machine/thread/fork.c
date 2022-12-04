#include <thread.h>
#include <assert.h>
#include <string.h>
#include <kprintf.h>
#include <spinlock.h>


/*
* Lol this should really be done in assembly.
*/
void arch_set_forked_kernel_stack(struct thread* original, struct thread* forked) {
    /*
    * Something's gone very wrong if fork() is changing the stack size.
    */
    assert(forked->kernel_stack_size == original->kernel_stack_size);

    /*
    * Get the value of EBP for this function, and the value the caller
    * had it set to. This is so we can set the stack pointers correctly.
    */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address"
    size_t current_stack_ptr = (size_t) __builtin_frame_address(0);
    size_t prev_stack_ptr = (size_t) __builtin_frame_address(1);
#pragma GCC diagnostic pop

    /*
    * The amount to add to convert an address on the parent stack to get
    * to the equivalent position in the child stack.
    */
    size_t stack_offset = forked->kernel_stack_top - original->kernel_stack_top;

    /*
    * Get the bottom of the stacks.
    */
    void* original_stack_base = (void*) (original->kernel_stack_top - original->kernel_stack_size);
    void* forked_stack_base = (void*) (forked->kernel_stack_top - forked->kernel_stack_size);

    /*
    * Copy the entire parent stack to the child.
    */
    memcpy(forked_stack_base, original_stack_base, forked->kernel_stack_size);

    /*
    * Adjust the stack pointer accordingly.
    */
    forked->stack_pointer = current_stack_ptr + stack_offset;

    /*
    * When we switch to the child thread, arch_switch_thread will pop some registers
    * off the stack and then return to the caller (thread_fork). We must fiddle
    * with the stack a bit to get these values on there.
    * 
    * EBX, ESI and EDI really should be saved (but it has to be the value *before*
    * we entered this stack frame, i.e. the values we saved on the stack at the start
    * of the function)
    */
    size_t* stack = (size_t*) forked->stack_pointer;
    *(--stack) = (size_t) __builtin_return_address(0);                  // return value
    *(--stack) = 0x0;                                                   // EBX
    *(--stack) = 0x0;                                                   // ESI
    *(--stack) = 0x0;                                                   // EDI
    *(--stack) = prev_stack_ptr + stack_offset;                         // EBP

    forked->stack_pointer -= 20;
}
