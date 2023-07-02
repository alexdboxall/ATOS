
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <cpu.h>
#include <arch.h>
#include <process.h>
#include <virtual.h>
#include <panic.h>
#include <uio.h>
#include <kprintf.h>

/*
* Gets, or changes the system break. The system break begins on a page boundary, 
* without any pages pre-allocated. The system break will always be on a page boundary,
* so changes may be rounded up. Subtracting from the system break might not be implemented,
* and even if it is, subtracting a value may not always have any effect. Subtracting a
* value less than the page size will never have an effect.
*
* Inputs: 
*         A                 the number of bytes to add to the system break
*         B                 if no-zero, subtracts the system break instead
*         C                 a pointer to a size_t, to hold a result: the system break before calling
*         D                 a pointer to a size_t, to hold a result: the system break after calling
* Output:
*         0                 on success
*         ENOMEM            on failure
*         ENOSYS            if B is non-zero
*/
int sys_sbrk(size_t args[4]) {
    if (args[1] != 0) {
        return ENOSYS;
    }

    kprintf("CALLING SBRK!\n");

    struct uio io = uio_construct_write_to_usermode((size_t*) args[2], sizeof(size_t), 0);
    size_t current_sbrk = current_cpu->current_thread->process->sbrk;
    int result = uio_move(&current_sbrk, &io, sizeof(size_t));
    if (result != 0) {
        return result;
    }

    size_t num_pages = virt_bytes_to_pages(args[0]);

    for (size_t i = 0; i < num_pages; ++i) {
        vas_reflag(vas_get_current_vas(), current_cpu->current_thread->process->sbrk + i * ARCH_PAGE_SIZE, VAS_FLAG_ALLOCATE_ON_ACCESS | VAS_FLAG_WRITABLE | VAS_FLAG_USER);
    }

    current_cpu->current_thread->process->sbrk += num_pages * ARCH_PAGE_SIZE;

    io = uio_construct_write_to_usermode((size_t*) args[3], sizeof(size_t), 0);
    size_t resulting_sbrk = current_cpu->current_thread->process->sbrk;
    kprintf("sbrk will return 0x%X\n", resulting_sbrk);
    return uio_move(&resulting_sbrk, &io, sizeof(size_t));
}