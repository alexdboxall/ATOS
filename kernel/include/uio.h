#pragma once

#include <common.h>

/*
* uio.h - User I/O
*
* Implemented in vfs/uio.c
*/

enum uio_type { 
    UIO_INTRA_KERNEL,
    UIO_USERMODE,
};

enum uio_direction {
    UIO_READ,
    UIO_WRITE,
};

/*
* A data structure for performing file read and write operations, potentially
* between the kernel and the user.
*
* TODO: userspace handling
*/
struct uio {
    void* address;
    size_t length_remaining;    /* In bytes. Will be modified on copying */
    size_t offset;              /* In bytes. Will be modified on copying */

    enum uio_direction direction;
    enum uio_type type;
};


int uio_move(void* trusted_buffer, struct uio* untrusted_buffer, size_t len);

/*
* max_length of 0 means unbounded
*/
int uio_move_string_to_usermode(char* trusted_string, char* untrusted_buffer, size_t max_length);
int uio_move_string_from_usermode(char* trusted_buffer, char* untrusted_string, size_t max_length);

struct uio uio_construct(void* addr, size_t length, size_t offset, int direction, int type);

#define uio_construct_kernel_write(addr, length, offset) uio_construct(addr, length, offset, UIO_WRITE, UIO_INTRA_KERNEL)
#define uio_construct_kernel_read(addr, length, offset) uio_construct(addr, length, offset, UIO_READ, UIO_INTRA_KERNEL)

/*
* "Reading" from the user is "writing" to the kernel (and vice versa), and we must do it this way around.
*/
#define uio_construct_write_to_usermode(addr, length, offset) uio_construct(addr, length, offset, UIO_READ, UIO_USERMODE)
#define uio_construct_read_from_usermode(addr, length, offset) uio_construct(addr, length, offset, UIO_WRITE, UIO_USERMODE)

