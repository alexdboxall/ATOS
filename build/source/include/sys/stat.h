#pragma once

#define S_IFMT	00100000
#define S_IFBLK	00200000
#define S_IFCHR	00300000
#define S_IFIFO	00400000
#define S_IFREG	00500000
#define S_IFDIR	00600000
#define S_IFLNK	00700000

#define _TYPE_MASK 0770000

#define S_IXOTH	00001
#define S_IWOTH	00002
#define S_IROTH	00004
#define S_IRWXO (S_IXOTH | S_IWOTH | S_IROTH)

#define S_IXGRP	00010
#define S_IWGRP	00020
#define S_IRGRP	00040
#define S_IRWXG (S_IXGRP | S_IWGRP | S_IRGRP)

#define S_IXUSR	00100
#define S_IWUSR	00200
#define S_IRUSR	00400
#define S_IRWXU (S_IXUSR | S_IWUSR | S_IRUSR)

#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define S_IREAD  S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC  S_IXUSR

#define S_ISBLK(m)  (((m) & _TYPE_MASK) == S_IFBLK)
#define S_ISCHR(m)  (((m) & _TYPE_MASK) == S_IFCHR)
#define S_ISDIR(m)  (((m) & _TYPE_MASK) == S_IFDIR)
#define S_ISFIFO(m) (((m) & _TYPE_MASK) == S_IFIFO)
#define S_ISLNK(m)  (((m) & _TYPE_MASK) == S_IFLNK)
#define S_ISREG(m)  (((m) & _TYPE_MASK) == S_IFREG)

#include <sys/types.h>

struct stat {

    /* The first two provide a unique ID for th file */
    dev_t st_dev;           /* ID of the device the file is on */
    ino_t st_ino;           /* File serial number */

    mode_t st_mode;         /* File permissions */
    nlink_t st_nlink;       /* Number of links to the file */
    uid_t st_uid;           /* File owner user ID */
    uid_t st_gid;           /* File group ID */
    dev_t st_rdev;          /* If S_IFCHR or S_IFCHR, the device ID */
    off_t st_size;          /* File size in bytes (only if S_IFREG) */
    time_t st_atime;        /* Last access time */
    time_t st_mtime;        /* Last modification time */
    time_t st_ctime;        /* Last status change time */
    blkcnt_t st_blocks;     /* Number of blocks allocated for file */
    blksize_t st_blksize;   /* Block size for file system I/O */
};