#pragma once

#include <common.h>

/*
* uio.h - User I/O
*
* Implemented in vfs/uio.c
*/

enum uio_direction {
    UIO_READ,
    UIO_WRITE
};

/*
* A data structure for performing file read and write operations, potentially
* between the kernel and the user.
*
* TODO: userspace handling
*
*/
struct uio {
    void* address;
    size_t length_remaining;    /* In bytes. Will be modified on copying */
    size_t offset;              /* In bytes. Will be modified on copying */

    enum uio_direction direction;
};


int uio_move(void* trusted_buffer, struct uio* untrusted_buffer, size_t len);
struct uio uio_construct_read(void* addr, size_t length, size_t offset);
struct uio uio_construct_write(void* addr, size_t length, size_t offset);