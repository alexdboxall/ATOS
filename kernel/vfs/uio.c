
#include <uio.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <kprintf.h>
#include <copyinout.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
* Copy data from kernel space to user space or vice versa, and for
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

    if (untrusted_buffer->type == UIO_INTRA_KERNEL) {
        if (untrusted_buffer->direction == UIO_READ) {
            memmove(untrusted_buffer->address, trusted_buffer, amount_to_copy);
        
        } else {
            memmove(trusted_buffer, untrusted_buffer->address, amount_to_copy);
        } 

    } else {
        int result;

        /*
        * This is from the kernel's perspective of the operations.
        */
        if (untrusted_buffer->direction == UIO_READ) {
            result = copyout((const void*) trusted_buffer, untrusted_buffer->address, amount_to_copy);
            
        } else {
            result = copyin(trusted_buffer, (const void*) untrusted_buffer->address, amount_to_copy);

        }

        if (result != 0) {
            return result;
        }
    }

    untrusted_buffer->length_remaining -= amount_to_copy;
    untrusted_buffer->offset += amount_to_copy;
    untrusted_buffer->address = ((uint8_t*) untrusted_buffer->address) + amount_to_copy;

    return 0;
}

int uio_move_string_to_usermode(char* trusted_string, char* untrusted_buffer, size_t max_length) {
    uint8_t zero = 0;

    struct uio uio = uio_construct_write_to_usermode(untrusted_buffer, max_length, 0);
    int result;

    if (strlen(trusted_string) < max_length) {
        result = uio_move(trusted_string, &uio, strlen(trusted_string));

    } else {
        result = uio_move(trusted_string, &uio, max_length - 1);
    }

    if (result != 0) {
        return result;
    }
    
    return uio_move(&zero, &uio, 1);
}

int uio_move_string_from_usermode(char* trusted_buffer, char* untrusted_string, size_t max_length) {
    char c;
    struct uio uio = uio_construct_read_from_usermode(untrusted_string, max_length, 0);
    size_t i = 0;

    while (max_length--) {
        int result = uio_move(&c, &uio, 1);
        if (result != 0) {
            return result;
        }
        trusted_buffer[i++] = c;
        if (c == 0) {
            break;
        }
    }
    
    return 0;
}

struct uio uio_construct(void* addr, size_t length, size_t offset, int direction, int type) {
    struct uio io;
    io.address = addr;
    io.length_remaining = length;
    io.offset = offset;
    io.direction = direction;
    io.type = type;
    return io;
}