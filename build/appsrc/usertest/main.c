
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
    fprintf(f, "Testing %s printf...\n", "simple");

    fprintf(f, ":%s:\n", "Hello, world!");
	fprintf(f, ":%15s:\n", "Hello, world!");
	fprintf(f, ":%.10s:\n", "Hello, world!");
	fprintf(f, ":%-10s:\n", "Hello, world!");
	fprintf(f, ":%-15s:\n", "Hello, world!");
	fprintf(f, ":%.15s:\n", "Hello, world!");
	fprintf(f, ":%15.10s:\n", "Hello, world!");
	fprintf(f, ":%-15.10s:\n\n", "Hello, world!");

    fprintf(f, "The color: %s\n", "blue");
	fprintf(f, "First number: %d\n", 12345);
	fprintf(f, "Second number: %04d\n", 25);
	fprintf(f, "Third number: %i\n", 1234);
	fprintf(f, "Hexadecimal: %x\n", 255);
	fprintf(f, "Octal: %o\n", 255);
	fprintf(f, "Unsigned value: %u\n", 150);
	fprintf(f, "Just print the percentage sign %%\n\n", 10);

    /*
EXPECTED

:Hello, world!:
:  Hello, world!:
:Hello, wor:
:Hello, world!:
:Hello, world!  :
:Hello, world!:
:     Hello, wor:
:Hello, wor     :

The color: blue
First number: 12345
Second number: 0025
Third number: 1234
Hexadecimal: ff
Octal: 377
Unsigned value: 150
Just print the percentage sign %

    */

    fprintf(f, ".%4d.\n", 13);
    fprintf(f, ".%+4d.\n", 13);
    fprintf(f, ".% 4d.\n", 13);
    fprintf(f, ".%-4d.\n", 13);
    fprintf(f, ".%04d.\n", 13);
    fprintf(f, ".% 04d.\n", 13);
    fprintf(f, ".%+04d.\n", 13);

    fprintf(f, ".%4d.\n", -13);
    fprintf(f, ".%+4d.\n", -13);
    fprintf(f, ".% 4d.\n", -13);
    fprintf(f, ".%-4d.\n", -13);
    fprintf(f, ".%04d.\n", -13);
    fprintf(f, ".% 04d.\n", -13);
    fprintf(f, ".%+04d.\n", -13);
    
    fprintf(f, ".%4.0d.\n", 13);
    fprintf(f, ".%4.1d.\n", 13);
    fprintf(f, ".%4.9d.\n", 13);

    fprintf(f, ".%4.0d.\n", 0);
    fprintf(f, ".%4.1d.\n", 0);
    fprintf(f, ".%4.9d.\n", 0);
/*
EXPECTED
.  13.
. +13.
.  13.
.13  .
.0013.
. 013.
.+013.
. -13.
. -13.
. -13.
.-13 .
.-013.
.-013.
.-013.
.  13.
.  13.
.000000013.
.    .
.   0.
.000000000.

*/

    fprintf(f, ".%*d.\n", 3, 123);
    fprintf(f, ".%*d.\n", 4, 123);
    fprintf(f, ".%*d.\n", 5, 123);
    fprintf(f, ".%*d.\n", 6, 123);
    fprintf(f, ".%.*d.\n", 0, 0);
    fprintf(f, ".%.*d.\n", 1, 0);

    fflush(f);
    fclose(f);

    while (1) {
        _system_call(SYSCALL_YIELD, 0, 0, 0, 0);
    }
}