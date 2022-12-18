#pragma once

#define O_CREAT		1
#define O_EXCL		2
#define O_NOCTTY	4
#define O_TRUNC		8

#define O_APPEND	16

#define O_RDONLY	32
#define O_WRONLY	64
#define O_RDWR		(O_RDONLY | O_WRONLY)
#define O_ACCMODE	(O_RDONLY | O_WRONLY)

#ifndef COMPILE_KERNEL

#include <sys/types.h>

#define F_DUPFD        0
#define F_GETFD        1
#define F_SETFD        2

int open(const char* filename, int flags, mode_t mode);
int creat(const char* fileame, mode_t mode);
int fcntl(int fd, int command);

#endif