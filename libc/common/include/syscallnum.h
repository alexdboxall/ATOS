#pragma once

enum SyscallNumber {
    SYSCALL_YIELD = 0,
    SYSCALL_TERMINATE,
    SYSCALL_OPEN,
    SYSCALL_READ,
    SYSCALL_WRITE,
    SYSCALL_CLOSE,
    SYSCALL_LSEEK,
    SYSCALL_SBRK,
    SYSCALL_ISATTY,
    SYSCALL_DUP,
    SYSCALL_DUP2,
    SYSCALL_DUP3,
    SYSCALL_TCGETATTR,
    SYSCALL_TCSETATTR
};

#ifndef COMPILE_KERNEL

#include <stddef.h>
int _system_call(int call, size_t arg1, size_t arg2, size_t arg3, size_t arg4); 

#endif