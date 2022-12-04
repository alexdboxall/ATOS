#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint32_t blkcnt_t;
typedef uint32_t blksize_t;
typedef uint64_t clock_t;
typedef uint32_t clockid_t;
typedef uint64_t dev_t;
typedef uint32_t fsblkcnt_t;
typedef uint32_t fsfilcnt_t;
typedef uint32_t gid_t;
typedef uint32_t id_t;
typedef uint32_t ino_t;
typedef uint32_t key_t;
typedef uint32_t mode_t;
typedef uint32_t nlink_t;
typedef uint32_t off_t;
typedef uint32_t pid_t;
typedef uint64_t suseconds_t;
typedef uint64_t time_t;
typedef uint64_t timer_t;
typedef uint32_t uid_t;
typedef uint32_t useconds_t;

#if (SIZE_MAX == 0xFFFFFFFF)
typedef int32_t ssize_t;
#elif (SIZE_MAX == 0xFFFFFFFFFFFFFFFF
typedef int64_t ssize_t;v
#else
#error "Strange SIZE_MAX, please define ssize_t yourself"
#endif