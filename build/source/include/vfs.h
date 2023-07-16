#pragma once

/*
* vfs.h - Virtual Filesystem
* 
* Implemented in vfs/vfs.c
*/

#include <common.h>
#include <interface.h>
#include <uio.h>
#include <sys/types.h>
#include <spinlock.h>

struct vnode;

struct open_file {
    bool can_read;
    bool can_write;
    mode_t initial_mode;
    size_t seek_position;
	int flags;
	int reference_count;
    struct spinlock reference_count_lock;

    struct vnode* node;
};

struct fs {
	struct open_file* raw_device;
	struct open_file* root_directory;
};

struct open_file* open_file_create(struct vnode* node, int mode, int flags, bool can_read, bool can_write);
void open_file_reference(struct open_file* file);
void open_file_dereference(struct open_file* file);


void vfs_init(void);
int vfs_add_device(struct std_device_interface* dev, const char* name);
int vfs_remove_device(const char* name);
int vfs_mount_filesystem(const char* filesystem_name, int (*vnode_creator)(struct open_file*, struct open_file**));
int vfs_open(const char* path, int flags, mode_t mode, struct open_file** out);
int vfs_read(struct open_file* node, struct uio* io);
int vfs_readdir(struct open_file* node, struct uio* io);
int vfs_write(struct open_file* node, struct uio* io);
int vfs_close(struct open_file* node);
int vfs_add_virtual_mount_point(const char* mount_point, const char* filename);