
#include <syscallnum.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

void _start() {
    FILE* f = fopen("con:", "w");
    for (int i = 0; "Hello world from usermode!"[i]; ++i) {
        fputc("Hello world from usermode!"[i], f);
    }
    fclose(f);


    //int fd = open("con:", O_WRONLY, 0);
    //write(fd, "Hello world from usermode!", 27);

    while (1) {
        _system_call(SYSCALL_YIELD, 0, 0, 0, 0);
    }
}