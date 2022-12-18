#include <fcntl.h>
#include <errno.h>

int open(const char* filename, int flags, mode_t mode) {
    (void) filename;
    (void) flags;
    (void) mode;

    errno = ENOSYS;

    return -1;
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