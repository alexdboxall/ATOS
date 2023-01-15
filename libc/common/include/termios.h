#pragma once

#include <stdint.h>

typedef uint16_t cc_t;
typedef uint32_t speed_t;
typedef uint32_t tcflag_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;

    cc_t c_cc[NCCS];
};

#define ECHO    1
#define ICANON  2

#define TCSANOW 1

#ifndef COMPILE_KERNEL
int tcgetattr(int fd, struct termios *term);
int tcsetattr(int fd, int optional_actions, const struct termios *term);
#endif