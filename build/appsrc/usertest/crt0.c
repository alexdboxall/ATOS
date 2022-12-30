#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <syscallnum.h>
#include <errno.h>

extern int main(int argc, char** argv);

void _start() {
    /*
    * TODO: malloc may need setting up in the future,
    */

    /*
    * Keep in this order, as stdin, stdout, stderr should be file descriptors
    * 0, 1 and 2 respectively.
    */
    stdin = fopen("con:", "r");
    stdout = fopen("con:", "w");
    stderr = fopen("con:", "w");

    /*
    * stderr must not have buffering enabled.
    */
    setvbuf(stderr, NULL, _IONBF, 1);

    /*
    * TODO: getting args
    */

    errno = 0;

    /*
    * Run the actual program and then pass the return code as the 
    * status returned to the OS.
    */
    exit(main(0, NULL));

    while (1) {
        _system_call(SYSCALL_YIELD, 0, 0, 0, 0);
    }
}
