
#include <syscallnum.h>
#include <fcntl.h>

void _start() {
    int fd;
    _system_call(SYSCALL_OPEN, (size_t) "con:", O_WRONLY, 0, (size_t) &fd);
    _system_call(SYSCALL_WRITE, (size_t) "Hello world from usermode!", 27, fd, 0);

    while (1) {
        _system_call(SYSCALL_YIELD, 0, 0, 0, 0);
    }
}