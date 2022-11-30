
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
    if (fs->disk->dev->block_size != SECTOR_SIZE) {
        /*
        * All you would have to do to support larger sectors is to tweak the 
        * struct uio* below to copy to a different buffer, and then copy
        * a subsection of that buffer to the buffer passed in. You will also
        * need to modify the sector number you use (maybe modify INDOE_TO_SECTOR?).
        */
        panic("demofs: sector_size != 512 not supported");
    }

    struct uio io = uio_construct_read(buffer, SECTOR_SIZE, SECTOR_SIZE * INODE_TO_SECTOR(inode));
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
            struct uio temp_io;
            temp_io.direction = UIO_READ;
            temp_io.address = sector_buffer;
            temp_io.length_remaining = SECTOR_SIZE;
            temp_io.offset = sector * SECTOR_SIZE;

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