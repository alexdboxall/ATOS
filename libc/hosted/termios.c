#include <termios.h>
#include <errno.h>
#include <syscallnum.h>

int tcgetattr(int fd, struct termios *term) {
    int result = _system_call(SYSCALL_TCGETATTR, fd, (size_t) term, 0, 0);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *term) {
    int result = _system_call(SYSCALL_TCSETATTR, fd, (size_t) term, optional_actions, 0);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return 0;
}