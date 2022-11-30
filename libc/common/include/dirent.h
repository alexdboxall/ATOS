#pragma once

/*
* dirent.h - Directory Entries
*
*/

#include <sys/types.h>

#define _DIRENT_HAVE_D_TYPE

#define DT_UNKNOWN  0
#define DT_REG      1
#define DT_DIR      2
#define DT_FIFO     3
#define DT_SOCK     4
#define DT_CHR      5
#define DT_BLK      6
#define DT_LNK      7

struct dirent {
    ino_t d_ino;
    char d_name[256];
    uint8_t d_namlen;
    uint8_t d_type;
};