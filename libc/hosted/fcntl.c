#include <fcntl.h>
#include <errno.h>
#include <syscallnum.h>

int open(const char* filename, int flags, mode_t mode) {
    int fd;
    int result = _system_call(SYSCALL_OPEN, (size_t) filename, flags, mode, (size_t) &fd);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return fd;
}

int creat(const char* filename, mode_t mode) {
    return open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int fcntl(int fd, int command) {
    (void) fd;
    (void) command;

    errno = ENOSYS;

    return -1;
}