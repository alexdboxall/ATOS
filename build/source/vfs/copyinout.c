
#include <copyinout.h>
#include <errno.h>
#include <string.h>
#include <virtual.h>
#include <arch.h>

static int validate_copy(const void* user_addr, size_t size, bool write) {
    size_t initial_address = (size_t) user_addr;

    /*
    * Check if the memory range starts in user memory.
    */
    if (initial_address < ARCH_USER_AREA_BASE || initial_address >= ARCH_USER_AREA_LIMIT) {
        return EINVAL;
    }

    size_t final_address = initial_address + size;

    /*
    * Check for overflow when the initial address and size are added. If it would overflow,
    * we cancel the operation, as the user is obviously outside their range.
    */
    if (final_address < initial_address) {
        return EINVAL;
    }

    /*
    * Ensure the end of the memory range is in user memory. As user memory must be contiguous,
    * this ensures the entire range is in user memory.
    */
    if (final_address < ARCH_USER_AREA_BASE || final_address >= ARCH_USER_AREA_LIMIT) {
        return EINVAL;
    }

    /*
    * We must now check if the USER (and possible WRITE) bits are set on the memory pages
    * being accessed.
    */
    size_t initial_page = initial_address / ARCH_PAGE_SIZE;
    size_t pages = (size + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;

    for (size_t i = 0; i < pages; ++i) {
        size_t page = initial_page + i;

        size_t phys;
        int flags;

        arch_vas_get_entry(vas_get_current_vas(), page * ARCH_PAGE_SIZE, &phys, &flags);

        if (!(flags & VAS_FLAG_PRESENT)) {
            // need to check if on the disk or not, or COW, etc.
            //
            // the easiest way might be to set a flag + struct telling the page fault handler to come
            // back here on the next page fault (clear the flag after we're done), and then we can just
            // touch the page and let the pf handler load it in if needed.

            // do some kind of lock...
            // setjmp...
            // tell the pf handler about the setjmp struct
            // touch the memory
            // check if the pf handler said "now that's bad" (i.e. read a flag)
            // stop telling the pf handler about the setjmp struct
            // unlock...
            // if "now that's bad", return EINVAL

            return ENOSYS;
        }
        
        /*
        * If it is kernel-only memory, we can't allow usermode to access it. 
        */
        if (!(flags & VAS_FLAG_USER)) {
            return EINVAL;
        }

        /*
        * If we're writing to usermode memory, but it isn't marked as writable,
        * we must also not write there. We also must prevent usermode from overwriting
        * its own code segment.
        */
        if (write && !(flags & VAS_FLAG_WRITABLE)) {
            return EINVAL;
        }
        if (write && (flags & VAS_FLAG_EXECUTABLE)) {
            return EINVAL;
        }
    }
    
    return 0;
}

int copyin(void* kernel_addr, const void* user_addr, size_t size) {
    int status = validate_copy(user_addr, size, false);
    if (status != 0) {
        return status;
    }

    memcpy(kernel_addr, user_addr, size);
    return 0;
}

int copyout(const void* kernel_addr, void* user_addr, size_t size) {
    int status = validate_copy(user_addr, size, true);
    if (status != 0) {
        return status;
    }

    memcpy(user_addr, kernel_addr, size);
    return 0;
}