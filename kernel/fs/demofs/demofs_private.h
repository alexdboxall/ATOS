#pragma once

#include <sys/types.h>

struct demofs {
    struct vnode* disk;
    ino_t root_inode;  
};

#define MAX_NAME_LENGTH 24

#define INODE_TO_SECTOR(inode) (inode & 0xFFFFFF)
#define INODE_IS_DIR(inode) (inode >> 31)
#define INODE_TO_DIR(inode) (inode | (1U << 31U))

int demofs_read_file(struct demofs* fs, ino_t file, uint32_t file_size, struct uio* io);
int demofs_follow(struct demofs* fs, ino_t parent, ino_t* child, const char* name, uint32_t* file_length_out);