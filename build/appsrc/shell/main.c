#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 512

void eval(const char* cmd_line) {
    printf("%s\n", cmd_line);
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    char buffer[MAXLINE];

    while (true) {
        printf("> ");
        fflush(stdout);

        fgets(buffer, MAXLINE - 1, stdin);
        if (feof(stdin)) {
            exit(0);
        }

        eval(buffer);
    }
}