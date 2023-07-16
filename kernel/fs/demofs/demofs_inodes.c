
#include <common.h>
#include <vnode.h>
#include <errno.h>
#include <vfs.h>
#include <string.h>
#include <assert.h>
#include <panic.h>
#include <uio.h>
#include <kprintf.h>
#include <sys/types.h>
#include <dirent.h>
#include <fs/demofs/demofs_private.h>

#define SECTOR_SIZE 512

/*
* We are going to use the high bit of an inode ID to indicate whether or not
* we are talking about a directory or not (high bit set = directory).
*
* This allows us to, for example, easily catch ENOTDIR in demofs_follow.
*
* Remember to use INODE_TO_SECTOR. Note that inodes are only stored using 24 bits
* anyway. 
*/

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int demofs_read_inode(struct demofs* fs, ino_t inode, uint8_t* buffer) {
    if (fs->disk->node->dev->block_size != SECTOR_SIZE) {
        /*
        * All you would have to do to support larger sectors is to tweak the 
        * struct uio* below to copy to a different buffer, and then copy
        * a subsection of that buffer to the buffer passed in. You will also
        * need to modify the sector number you use (maybe modify INDOE_TO_SECTOR?).
        */
        panic("demofs: sector_size != 512 not supported");
    }

    struct uio io = uio_construct_kernel_read(buffer, SECTOR_SIZE, SECTOR_SIZE * INODE_TO_SECTOR(inode));
    return vfs_read(fs->disk, &io);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int demofs_read_file(struct demofs* fs, ino_t file, uint32_t file_size_left, struct uio* io) {
    if (io->offset >= file_size_left) {
        return 0;
    }

    file_size_left -= io->offset;

    while (io->length_remaining != 0 && file_size_left != 0) {
        int sector = file + io->offset / SECTOR_SIZE;
        int sector_offset = io->offset % SECTOR_SIZE;

        if (sector_offset == 0 && io->length_remaining >= SECTOR_SIZE && file_size_left >= SECTOR_SIZE) {
            /*
            * We have an aligned sector amount, so transfer it all directly,
            * execpt for possible a few bytes at the end.
            *
            * vfs_read only allows lengths that are a multiple of the sector
            * size, so round down to the nearest sector. The remainder must be 
            * kept track of so it can be added back on after the read.
            */
           
            int remainder = io->length_remaining % SECTOR_SIZE;
            io->length_remaining -= remainder;

            /* 
            * We need the disk offset, not the file offset.
            * Ensure we move it back though afterwards.
            */
            int delta = sector * SECTOR_SIZE - io->offset;
            io->offset += delta;

            int status = vfs_read(fs->disk, io);
            if (status != 0) {
                return status;
            }

            io->offset -= delta;
            io->length_remaining = remainder;
            file_size_left -= SECTOR_SIZE;

        } else {
            /*
            * A partial sector transfer.
            *
            * We must read the sector into an internal buffer, and then copy a
            * subsection of that to the return buffer.
            */
            uint8_t sector_buffer[SECTOR_SIZE];

            /* Read the sector */
            struct uio temp_io = uio_construct_kernel_read(sector_buffer, SECTOR_SIZE, sector * SECTOR_SIZE);

            int status = vfs_read(fs->disk, &temp_io);
            if (status != 0) {
                return status;
            }

            /* Transfer to the correct buffer */
            size_t transfer_size = MIN(MIN(SECTOR_SIZE - (io->offset % SECTOR_SIZE), io->length_remaining), file_size_left);
            uio_move(sector_buffer + (io->offset % SECTOR_SIZE), io, transfer_size);
            file_size_left -= transfer_size;
        }
    }
    
    return 0;
}

int demofs_follow(struct demofs* fs, ino_t parent, ino_t* child, const char* name, uint32_t* file_length_out) {
    assert(fs);
    assert(fs->disk);
    assert(child);
    assert(name);
    assert(file_length_out);
    assert(SECTOR_SIZE % 32 == 0);

    uint8_t buffer[SECTOR_SIZE];

    if (strlen(name) > MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    if (!INODE_IS_DIR(parent)) {
        return ENOTDIR;
    }

    /*
    * The directory may contain many entries, so we need to iterate through them.
    */
    while (true) {
        /*
        * Grab the current entry.
        */
        int status = demofs_read_inode(fs, parent, buffer);
        if (status != 0) {
            return status;
        }

        /*
        * Something went very wrong if the directory header is not present!
        */
        if (buffer[0] != 0xFF && buffer[0] != 0xFE) {
            return EIO;
        }

        for (int i = 1; i < SECTOR_SIZE / 32; ++i) {
            /*
            * Check if there are no more names in the directory.
            */
            if (buffer[i * 32] == 0) {
                return ENOENT;
            }

            /*
            * Check if we've found the name.
            */
            if (!strncmp(name, (char*) buffer + i * 32, MAX_NAME_LENGTH)) {
                /*
                * If so, read the inode number and return it.
                * Remember to add the directory flag if necessary.
                */
                ino_t inode = buffer[i * 32 + MAX_NAME_LENGTH + 4];
                inode |= (ino_t) buffer[i * 32 + MAX_NAME_LENGTH + 5] << 8;
                inode |= (ino_t) buffer[i * 32 + MAX_NAME_LENGTH + 6] << 16;

                if (buffer[i * 32 + MAX_NAME_LENGTH + 7] & 1) {
                    /*
                    * This is a directory.
                    */
                    inode = INODE_TO_DIR(inode);
                    *file_length_out = 0;

                } else {
                    /*
                    * This is a file.
                    */
                    uint32_t length = buffer[i * 32 + MAX_NAME_LENGTH];
                    length |= (uint32_t) buffer[i * 32 + MAX_NAME_LENGTH + 1] << 8;
                    length |= (uint32_t) buffer[i * 32 + MAX_NAME_LENGTH + 2] << 16;
                    length |= (uint32_t) buffer[i * 32 + MAX_NAME_LENGTH + 3] << 24;

                    *file_length_out = length;
                }

                *child = inode;
                return 0;
            }
        }

        /*
        * Now we need to move on to the next entry if there is one.
        */
        if (buffer[0] == 0xFF) {
            /* No more entries. */
            return ENOENT;

        } else if (buffer[0] == 0xFE) {
            /*
            * There is another entry, so read its inode and keep the loop going
            */
            parent = buffer[1];
            parent |= (ino_t) buffer[2] << 8;
            parent |= (ino_t) buffer[3] << 16;

            /*
            * Add the directory bit to the inode number as it should be a directory.
            */
            parent = INODE_TO_DIR(parent);

        } else {
            /*
            * Something went very wrong if the directory header is not present!
            */
            return EIO;
        }
    }
}

int demofs_read_directory_entry(struct demofs* fs, ino_t directory, struct uio* io) {

        /*
    *   readdir
*           Reads a directory entry into a struct dirent. The offset in the struct uio
*           will be a multiple of sizeof(struct dirent), and determines which directory
*           entry to use. If the offset is beyond the end of the directory, no transfer
*           should occur, and thus the bytes remaining should still be sizeof(struct dirent).
    */

    if (!INODE_IS_DIR(directory)) {
        return ENOTDIR;
    }

    assert(SECTOR_SIZE % 32 == 0);
    uint8_t buffer[SECTOR_SIZE];

    struct dirent dir;
    assert(io->offset % sizeof(struct dirent) == 0);
    int entry_number = io->offset / sizeof(struct dirent);

    /*
    * Each directory inode contains 31 files, and a pointer to the next directory entry.
    * Add 1 to the offset to skip past the header.
    */
    int indirections = entry_number / 31;
    int offset = entry_number % 31 + 1;

    /*
    * Get the correct inode 
    */
    int status = 0;
    ino_t current_inode = directory;
    for (int i = 0; i < indirections; ++i) {
        status = demofs_read_inode(fs, current_inode, buffer);
        if (status != 0) {
            return status;
        }

        /*
        * Check for end of directory.
        */
        if (buffer[0] == 0xFF) {
            return 0;
        }

        if (buffer[0] != 0xFE) {
            return EIO;
        }

        /*
        * Get the next in the chain.
        */
        current_inode = buffer[1];
        current_inode |= (ino_t) buffer[2] << 8;
        current_inode |= (ino_t) buffer[3] << 16;
    }

    status = demofs_read_inode(fs, current_inode, buffer);
    if (status != 0) {
        return status;
    }

    /*
    * Check if we've gone past the end of the directory.
    */
    if (buffer[offset * 32] == 0) {
        return 0;
    }

    /*
    * strncpy is a bit iffy, so let's just play it safe.
    */
    char name[MAX_NAME_LENGTH + 2];
    memset(name, 0, MAX_NAME_LENGTH + 2);
    strncpy(name, (char*) buffer + offset * 32, MAX_NAME_LENGTH);
    strcpy(dir.d_name, name);

    dir.d_namlen = strlen(name);

    ino_t inode = buffer[offset * 32 + MAX_NAME_LENGTH + 4];
    inode |= (ino_t) buffer[offset * 32 + MAX_NAME_LENGTH + 5] << 8;
    inode |= (ino_t) buffer[offset * 32 + MAX_NAME_LENGTH + 6] << 16;

    dir.d_ino = inode;
    dir.d_type = INODE_IS_DIR(inode) ? DT_DIR : DT_REG;

    /* Perform the transfer to the correct location */
    return uio_move(&dir, io, sizeof(struct dirent));
}