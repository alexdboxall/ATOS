
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>     // <heap.h>
#include <stdio.h>      // <kprintf.h>
#include <time.h>


typedef int inode_t;


struct lock
{
    int lock;
};

void lock_init(struct lock* l)
{

}

void lock_acquire(struct lock* l)
{

}

void lock_release(struct lock* l)
{

}

#define MAX_NAME_LENGTH             24
#define FLAG_DIR                    (1 << 0)
#define SECTOR_SIZE                 512

struct demofs_data
{
    int err;
    struct lock lock;
    inode_t total_logical_sectors;
    inode_t root_directory_inode;
    inode_t kernel_image_inode;
    uint8_t kernel_num_kilobytes;
};


uint8_t* dummy_disk;


int demofs_read_sector(uint8_t* buffer, int logical_sector)
{
    /*
    * TODO: call the VFS, caching?
    */
    memcpy(buffer, dummy_disk + logical_sector * SECTOR_SIZE, SECTOR_SIZE);
    return 0;
}


int demofs_write_sector(uint8_t* buffer, int logical_sector)
{
    /*
    * TODO: call the VFS, caching?
    */
    memcpy(dummy_disk + logical_sector * SECTOR_SIZE, buffer, SECTOR_SIZE);
    return 0;
}

void demofs_format(struct demofs_data* fs)
{
    lock_acquire(&fs->lock);

    fs->root_directory_inode = 9;

    uint8_t buffer[SECTOR_SIZE];
    memset(buffer, 0, SECTOR_SIZE);
    demofs_write_sector(buffer, 0);

    strcpy((char*) buffer, "DEMO");
    buffer[4] = 0;

    uint8_t sector_size_power = 0;
    while (SECTOR_SIZE != (1 << sector_size_power)) {
        ++sector_size_power;
        if (sector_size_power == 255) {
            abort();
        }
    }

    buffer[5] = sector_size_power;
    buffer[6] = fs->root_directory_inode & 0xFF;
    buffer[7] = (fs->root_directory_inode >> 8) & 0xFF;
    buffer[8] = (fs->root_directory_inode >> 16) & 0xFF;

    buffer[9] = fs->total_logical_sectors & 0xFF;
    buffer[10] = (fs->total_logical_sectors >> 8) & 0xFF;
    buffer[11] = (fs->total_logical_sectors >> 16) & 0xFF;

    buffer[12] = fs->kernel_image_inode & 0xFF;
    buffer[13] = (fs->kernel_image_inode >> 8) & 0xFF;
    buffer[14] = (fs->kernel_image_inode >> 16) & 0xFF;

    buffer[15] = fs->kernel_num_kilobytes;

    demofs_write_sector(buffer, 8);


    memset(buffer, 0, SECTOR_SIZE);
    buffer[0] = 0xFF;
    demofs_write_sector(buffer, fs->root_directory_inode);

    lock_release(&fs->lock);
}

int demofs_each_open(struct demofs_data* fs, const char* name, int flags)
{
    if (strlen(name) >= MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    if ((flags & O_ACCMODE) == O_RDONLY || (flags & O_ACCMODE) == O_RDWR) {
        return EROFS;
    }
    if (flags & O_CREAT) {
        return EROFS;
    }
    if (flags & O_TRUNC) {
        return EROFS;
    }
    if (flags & O_APPEND) {
        return EROFS;
    }

    return 0;
}

static int next_inode = 10;

int demofs_mkdir(int parent, const char* name)
{
    uint8_t buffer[SECTOR_SIZE];

    demofs_read_sector(buffer, parent);

    for (int i = 1; i < SECTOR_SIZE / 32; ++i) {
        if (buffer[i * 32] == 0) {
            strncpy((char*) buffer + i * 32, name, MAX_NAME_LENGTH);
            buffer[i * 32 + MAX_NAME_LENGTH + 4] = (next_inode >> 0) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 5] = (next_inode >> 8) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 6] = (next_inode >> 16) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 7] = 1;

            demofs_write_sector(buffer, parent);

            memset(buffer, 0, SECTOR_SIZE);
            buffer[0] = 0xFF;
            int ret_inode = next_inode;
            demofs_write_sector(buffer, next_inode++);

            return ret_inode;
        }
    }

    if (buffer[0] == 0xFE) {
        int parent_next = buffer[1];
        parent_next |= (uint32_t) buffer[2] << 8;
        parent_next |= (uint32_t) buffer[3] << 16;
        return demofs_mkdir(parent_next, name);

    } else if (buffer[0] == 0xFF) {
        int parent_next = next_inode++;
        buffer[0] = 0xFE;
        buffer[1] = (parent_next >> 0) & 0xFF;
        buffer[2] = (parent_next >> 8) & 0xFF;
        buffer[3] = (parent_next >> 16) & 0xFF;
        demofs_write_sector(buffer, parent);

        memset(buffer, 0, SECTOR_SIZE);
        buffer[0] = 0xFF;
        demofs_write_sector(buffer, parent_next);
        return demofs_mkdir(parent_next, name);

    } else {
        printf("cannot create directory!\n");
        abort();
        return -1;
    }
}

inode_t demofs_write(int parent, const char* name, const void* data, int length)
{
    uint8_t buffer[SECTOR_SIZE];

    demofs_read_sector(buffer, parent);

    for (int i = 1; i < SECTOR_SIZE / 32; ++i) {
        if (buffer[i * 32] == 0) {

            strncpy((char*) buffer + i * 32, name, MAX_NAME_LENGTH);
            buffer[i * 32 + MAX_NAME_LENGTH + 0] = (length >> 0) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 1] = (length >> 8) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 2] = (length >> 16) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 3] = (length >> 24) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 4] = (next_inode >> 0) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 5] = (next_inode >> 8) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 6] = (next_inode >> 16) & 0xFF;
            buffer[i * 32 + MAX_NAME_LENGTH + 7] = 0;

            demofs_write_sector(buffer, parent);

            inode_t ret_inode = next_inode;

            int num_sects = (length + SECTOR_SIZE - 1) / SECTOR_SIZE;
            for (int j = 0; j < num_sects; ++j) {
                memset(buffer, 0, SECTOR_SIZE);
                memcpy(buffer, (uint8_t*) data + j * SECTOR_SIZE, length > SECTOR_SIZE ? SECTOR_SIZE : length);
                demofs_write_sector(buffer, next_inode++);
                length -= SECTOR_SIZE;
            }

            return ret_inode;
        }
    }

    if (buffer[0] == 0xFE) {
        int parent_next = buffer[1];
        parent_next |= (uint32_t) buffer[2] << 8;
        parent_next |= (uint32_t) buffer[3] << 16;
        return demofs_write(parent_next, name, data, length);

    } else if (buffer[0] == 0xFF) {
        int parent_next = next_inode++;
        buffer[0] = 0xFE;
        buffer[1] = (parent_next >> 0) & 0xFF;
        buffer[2] = (parent_next >> 8) & 0xFF;
        buffer[3] = (parent_next >> 16) & 0xFF;
        demofs_write_sector(buffer, parent);

        memset(buffer, 0, SECTOR_SIZE);
        buffer[0] = 0xFF;
        demofs_write_sector(buffer, parent_next);
        return demofs_write(parent_next, name, data, length);

    } else {
        printf("cannot create file!\n");
        abort();
        return -1;
    }
}


#include <stdio.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

int kernel_inode = 0;

void listdir(struct demofs_data* fs, const char* name, const char* shortname, int parent)
{
    DIR* dir;
    struct dirent* entry;

    if (!(dir = opendir(name)))
        return;

    int inode = parent != -1 ? demofs_mkdir(parent, shortname) : fs->root_directory_inode;

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);

        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            listdir(fs, path, entry->d_name, inode);
        } else {
            // TODO: write file

            FILE* f = fopen(path, "rb");
            fseek(f, 0, SEEK_END);
            long pos = ftell(f);
            rewind(f);
            char* buff = (char*) malloc(pos + 1);
            fread(buff, 1, pos, f);
            fclose(f);

            inode_t i = demofs_write(inode, entry->d_name, buff, pos);

            if (!strcmp(entry->d_name, "KERNEL.EXE") || !strcmp(entry->d_name, "kernel.exe")) {
                kernel_inode = i;
            }

            free(buff);
        }
    }
    closedir(dir);
}

void demofs_add_kernel(struct demofs_data* fs, const char* path)
{
    FILE* f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    long pos = ftell(f);
    rewind(f);
    char* buff = (char*) malloc(pos + 1);
    fread(buff, 1, pos, f);
    fclose(f);

    if (pos >= 1024 * 255) {
		printf("kernel too big!\n");
		abort();
    }
	
    fs->kernel_image_inode = kernel_inode; //demofs_write(fs->root_directory_inode, name, buff, pos);
    fs->kernel_num_kilobytes = (pos + 1023) / 1024;

    uint8_t buffer[SECTOR_SIZE];
    demofs_read_sector(buffer, 8);

    buffer[12] = fs->kernel_image_inode & 0xFF;
    buffer[13] = (fs->kernel_image_inode >> 8) & 0xFF;
    buffer[14] = (fs->kernel_image_inode >> 16) & 0xFF;
    buffer[15] = fs->kernel_num_kilobytes;
    demofs_write_sector(buffer, 8);

    free(buff);
}

int main(int argc, char** argv)
{
    if (argc != 4 && argc != 5 && argc != 6) {
        printf("Usage:\n\n    demofs megabytes kernel_filepath in_folder [out_file] [bootloader]\n\n");
        return 1;
    }

    int megabytes = atoi(argv[1]);

    struct demofs_data fs;
    fs.err = 0;
    fs.total_logical_sectors = megabytes * 1024 / SECTOR_SIZE * 1024;

    lock_init(&fs.lock);

    dummy_disk = (uint8_t*) malloc(fs.total_logical_sectors * SECTOR_SIZE);

    demofs_format(&fs);

    listdir(&fs, argv[3], "", -1);

    demofs_add_kernel(&fs, argv[2]);

    if (argc == 6) {
        FILE* f = fopen(argv[5], "rb");
        char* buff = (char*) malloc(SECTOR_SIZE * 8);
        fread(buff, 1, SECTOR_SIZE * 8, f);
        fclose(f);
        for (int i = 0; i < 8; ++i) {
            demofs_write_sector((uint8_t*) buff + SECTOR_SIZE * i, i);
        }
        free(buff);
    }

    FILE* f = fopen(argc >= 5 ? argv[4] : "output.bin", "wb");
    fwrite(dummy_disk, SECTOR_SIZE, fs.total_logical_sectors, f);
    fclose(f);

    return 0;
}