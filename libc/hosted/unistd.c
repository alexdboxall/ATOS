#include <unistd.h>
#include <errno.h>
#include <syscallnum.h>

int close(int fd) {
    (void) fd;

    errno = ENOSYS;

    return -1;
}

ssize_t read(int fd, void* buffer, size_t size) {
    int br;
    int result = _system_call(SYSCALL_READ, (size_t) buffer, size, fd, (size_t) &br);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return br;
}

ssize_t write(int fd, const void* buffer, size_t size) {
    int br;
    int result = _system_call(SYSCALL_WRITE, (size_t) buffer, size, fd, (size_t) &br);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return br;
}

off_t lseek(int fd, off_t offset, int whence) {
    (void) fd;
    (void) offset;
    (void) whence;

    errno = ENOSYS;

    return -1;
}