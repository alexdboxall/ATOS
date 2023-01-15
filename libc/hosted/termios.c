#include <termios.h>
#include <errno.h>

int tcgetattr(int fd, struct termios *term) {
    (void) fd;
    (void) term;

    errno = ENOSYS;
    return -1;
}

int tcsetattr(int fd, int optional_actions, const struct termios *term) {
    (void) fd;
    (void) optional_actions;
    (void) term;

    errno = ENOSYS;
    return -1;
}