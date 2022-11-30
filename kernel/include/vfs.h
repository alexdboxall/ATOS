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

struct vnode;

struct fs {
	struct vnode* raw_device;
	struct vnode* root_directory;
};

void vfs_init(void);
int vfs_add_device(struct std_device_interface* dev, const char* name);
int vfs_remove_device(const char* name);
int vfs_mount_filesystem(const char* filesystem_name, int (*vnode_creator)(struct vnode*, struct vnode**));
int vfs_open(const char* path, int flags, mode_t mode, struct vnode** out);
int vfs_read(struct vnode* node, struct uio* io);
int vfs_write(struct vnode* node, struct uio* io);
int vfs_close(struct vnode* node);
int vfs_add_virtual_mount_point(const char* mount_point, const char* filename);