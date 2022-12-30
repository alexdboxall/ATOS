#include <unistd.h>
#include <errno.h>
#include <syscallnum.h>

int isatty(int fd) {
    int result = _system_call(SYSCALL_ISATTY, fd, 0, 0, 0);
    if (result == 0) {
        return 1;
    }

    errno = result;
    return 0;
}

int close(int fd) {
    int result = _system_call(SYSCALL_CLOSE, fd, 0, 0, 0);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return 0;
}

ssize_t read(int fd, void* buffer, size_t size) {
    size_t br;
    int result = _system_call(SYSCALL_READ, (size_t) buffer, size, fd, (size_t) &br);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return br;
}

ssize_t write(int fd, const void* buffer, size_t size) {
    size_t br;
    int result = _system_call(SYSCALL_WRITE, (size_t) buffer, size, fd, (size_t) &br);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return br;
}

off_t lseek(int fd, off_t offset, int whence) {
    /*
    * Inputs: 
*         A                 the file descriptor
*         B                 a pointer to an off_t (the offset), the resulting offset is written back here
*         C                 either SEEK_SET, SEEK_CUR or SEEK_END
*         D                 not used
* Output:
*         0                 on success
*         error code        on failure*/

    off_t working_var = offset;
    int result = _system_call(SYSCALL_LSEEK, fd, (size_t) &working_var, whence, 0);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return working_var;
}