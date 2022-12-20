
#include <syscallnum.h>
#include <fcntl.h>
#include <unistd.h>

void _start() {
    int fd = open("con:", O_WRONLY, 0);
    write(fd, "Hello world from usermode!", 27);

    while (1) {
        _system_call(SYSCALL_YIELD, 0, 0, 0, 0);
    }
}