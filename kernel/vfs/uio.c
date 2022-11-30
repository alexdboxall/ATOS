
#include <uio.h>
#include <assert.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
* Copy data from kernel space to user space or vice versa. May also be used 
* for intra-kernel copying. 
*
* TODO: when usermode comes, check the transfer type. If kernel->kernel, do as
*       it is now. Otherwise use copyin / copyout functions (these do the validation
        with setjmp and all that)
*/
int uio_move(void* trusted_buffer, struct uio* untrusted_buffer, size_t len) {
    
    assert(trusted_buffer != NULL);
    assert(untrusted_buffer != NULL && untrusted_buffer->address != NULL);
    assert(untrusted_buffer->direction == UIO_READ || untrusted_buffer->direction == UIO_WRITE);

    size_t amount_to_copy = MIN(len, untrusted_buffer->length_remaining);
    if (amount_to_copy == 0) {
        return 0;
    }

    if (untrusted_buffer->direction == UIO_READ) {
        memmove(untrusted_buffer->address, trusted_buffer, amount_to_copy);
    
    } else {
        memmove(trusted_buffer, untrusted_buffer->address, amount_to_copy);
    }

    untrusted_buffer->length_remaining -= amount_to_copy;
    untrusted_buffer->offset += amount_to_copy;
    untrusted_buffer->address = ((uint8_t*) untrusted_buffer->address) + amount_to_copy;

    return 0;
}

struct uio uio_construct_read(void* addr, size_t length, size_t offset) {
    struct uio io;
    io.address = addr;
    io.length_remaining = length;
    io.offset = offset;
    io.direction = UIO_READ;
    return io;
}

struct uio uio_construct_write(void* addr, size_t length, size_t offset) {
    struct uio io;
    io.address = addr;
    io.length_remaining = length;
    io.offset = offset;
    io.direction = UIO_WRITE;
    return io;
}