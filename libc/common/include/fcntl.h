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

#define S_IRUSR     0400
#define S_IREAD     S_IRUSR
#define S_IWUSR     0200
#define S_IWRITE    S_IWUSR
#define S_IXUSR     0100
#define S_IEXEC     S_IXUSR
#define S_IRWXU     (S_IRUSR | S_IWUSR | S_IXUSR)

#define S_IRGRP     0040
#define S_IWGRP     0020
#define S_IXGRP     0010
#define S_IRWXG     (S_IRGRP | S_IWGRP | S_IXGRP)

#define S_IROTH     0004
#define S_IWOTH     0002
#define S_IXOTH     0001
#define S_IRWXO     (S_IROTH | S_IWOTH | S_IXOTH)

#define S_ISUID     04000
#define S_ISGID     02000
#define S_ISVTX     01000

#ifndef COMPILE_KERNEL

#include <sys/types.h>

#define F_DUPFD        0
#define F_GETFD        1
#define F_SETFD        2

int open(const char* filename, int flags, mode_t mode);
int creat(const char* fileame, mode_t mode);
int fcntl(int fd, int command);

#endif