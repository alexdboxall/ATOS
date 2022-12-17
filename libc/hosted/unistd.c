#include <unistd.h>
#include <errno.h>

int close(int fd) {
    (void) fd;

    errno = ENOSYS;

    return -1;
}

ssize_t read(int fd, void* buffer, size_t size) {
    (void) fd;
    (void) buffer;
    (void) size;

    errno = ENOSYS;

    return -1;
}

ssize_t write(int fd, void* buffer, size_t size) {
    (void) fd;
    (void) buffer;
    (void) size;

    errno = ENOSYS;

    return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
    (void) fd;
    (void) offset;
    (void) whence;

    errno = ENOSYS;

    return -1;
}