
#include <syscallnum.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

void _start() {
    FILE* f = fopen("con:", "w");
    fputs("Hello world from usermode!\n", f);

    fprintf(f, "Testing simple printf...\n");
    fprintf(f, "Testing %d printf...\n", 123);
    //fprintf(f, "Testing %s printf...\n", "simple");
    fflush(f);
    fclose(f);

    while (1) {
        _system_call(SYSCALL_YIELD, 0, 0, 0, 0);
    }
}