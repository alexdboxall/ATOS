
#include <heap.h>
#include <device.h>
#include <vnode.h>
#include <kprintf.h>
#include <assert.h>
#include <device.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

/*
* vfs/device.c - Link between device vnodes and device interfaces
*
* This file used to create a vnode which represents a device. 
*
* The VFS uses vnodes, which expect read/write/close/check_open/ioctl/seek/follow
* Drivers use std_device_interface, which expects io/ioctl/check_open.
* This file contains wrappers between them, and fills up a struct vnode* with
* function pointers to these wrappers.
*
* Device Specific Code  <--------------->  vfs/device.c  <-------->  vfs/vnode.c  <------->  vfs/vfs.c
*                                       (wrapper functions)
*                               ^                             ^            ^
*                               |                             |            |
*                               |                             |            \-------- wrapper functions
*                              via                           via                      on struct vnode*
*                 struct std_device_interface*        struct vnode_ops
*                 (set by device driver init)         (set by this file!)     
*/                         

static int dev_check_open(struct vnode* node, const char* name, int flags) {
    assert(node != NULL && node->dev != NULL && node->dev->check_open != NULL);

    return node->dev->check_open(node->dev, name, flags);
}

static int dev_ioctl(struct vnode* node, int command, void* buffer) {
    assert(node != NULL && node->dev != NULL && node->dev->ioctl != NULL);

    return node->dev->ioctl(node->dev, command, buffer);
}

static bool dev_isseekable(struct vnode* node) {
    assert(node != NULL && node->dev != NULL);

    return node->dev->block_size != 0;
}

static int dev_istty(struct vnode* node) {
    assert(node != NULL && node->dev != NULL);

    return node->dev->termios != NULL;
}

static int dev_read(struct vnode* node, struct uio* io) {
    assert(node != NULL && node->dev != NULL && node->dev->io != NULL);  
    assert(io->direction == UIO_READ);
    
    return node->dev->io(node->dev, io);
}

static int dev_write(struct vnode* node, struct uio* io) {
    assert(node != NULL && node->dev != NULL && node->dev->io != NULL);  
    assert(io->direction == UIO_WRITE);

    return node->dev->io(node->dev, io);
}

static int dev_create(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode) {
    (void) node;
    (void) out;
    (void) name;
    (void) flags;
    (void) mode;

    return ENOTDIR;
}

static uint8_t dev_dirent_type(struct vnode* node) {
    if (node->dev->block_size == 0) {
        return DT_CHR;
    } else {
        return DT_BLK;
    }
}

static int dev_stat(struct vnode* node, struct stat* st) {
    st->st_mode = (node->dev->block_size == 0 ? S_IFCHR : S_IFBLK) | S_IRWXU | S_IRWXG | S_IRWXO;
    st->st_atime = 0;
    st->st_blksize = node->dev->block_size;
    st->st_blocks = node->dev->num_blocks;
    st->st_ctime = 0;
    st->st_dev = 0xBABECAFE;
    st->st_gid = 0;
    st->st_ino = 0xCAFEBABE;
    st->st_mtime = 0;
    st->st_nlink = 1;
    st->st_rdev = 0xCAFEDEAD;
    st->st_size = st->st_blksize * st->st_blocks;
    st->st_uid = 0;

    return 0;
}

static int dev_truncate(struct vnode* node, off_t location) {
    /*
    * We can't really 'truncate' a device, but if the truncate location passed
    * in happens to be the length of the disk, the truncate operation wouldn't do
    * anything anyway. In this case, we just return success anyway.
    */
    if (node->dev->num_blocks != 0) {
        size_t total_size = node->dev->num_blocks * node->dev->block_size;

        if (total_size == location) {
            return 0;
        }
    }

    return EINVAL;
}

/*
* Devices will always be in the file tree, so no need to do anything.
*/
static int dev_close(struct vnode* node) {
    (void) node;
    return 0;
}

/*
* Devices can't have subdirectories or files!
*/
static int dev_follow(struct vnode* node, struct vnode** out, const char* name) {
    (void) node;
    (void) out;
    (void) name;

    return EINVAL;
}

static int dev_readdir(struct vnode* node, struct uio* io) {
    (void) node;
    (void) io;

    return EINVAL;
}

static const struct vnode_operations dev_ops = {
    .check_open     = dev_check_open,
    .ioctl          = dev_ioctl,
    .is_seekable    = dev_isseekable,
    .is_tty         = dev_istty,
    .read           = dev_read,
    .write          = dev_write,
    .close          = dev_close,
    .truncate       = dev_truncate,
    .create         = dev_create,
    .follow         = dev_follow,
    .dirent_type    = dev_dirent_type,
    .readdir        = dev_readdir,
    .stat           = dev_stat,
};


/*
* Creates a vnode with operations which wrap the given device interface.
* Sets the reference count to 1.
*/
struct vnode* dev_create_vnode(struct std_device_interface* dev)
{
    assert(dev != NULL);
	struct vnode* node = vnode_init(dev, dev_ops);

    /*
    * We won't be able to mount filesystems to devices without this.
    */
    node->can_read = true;
    node->can_write = true;

    node->flags = 0;
    node->initial_mode = 0;
    
    return node;
}
