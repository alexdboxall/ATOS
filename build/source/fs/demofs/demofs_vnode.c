
#include <heap.h>
#include <device.h>
#include <vnode.h>
#include <kprintf.h>
#include <assert.h>
#include <device.h>
#include <errno.h>
#include <fcntl.h>
#include <vnode.h>
#include <uio.h>
#include <adt.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <fs/demofs/demofs_private.h>

struct vnode_data {
    ino_t inode;
    struct demofs fs;
    uint32_t file_length;
};

static int demofs_vnode_ioctl(struct vnode* node, int command, void* buffer) {
    (void) node;
    (void) command;
    (void) buffer;

    return ENOSYS;
}

static bool demofs_vnode_isseekable(struct vnode* node) {
    (void) node;
    
    return true;
}

static int demofs_vnode_write(struct vnode* node, struct uio* io) {
    (void) node;
    (void) io;

    return EROFS;
}

static int demofs_vnode_create(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode) {
    (void) node;
    (void) out;
    (void) name;
    (void) flags;
    (void) mode;

    return EROFS;
}

static int demofs_vnode_truncate(struct vnode* node, off_t location) {
    (void) node;
    (void) location;

    return EROFS;
}

static int demofs_vnode_follow_file(struct vnode* node, struct vnode** out, const char* name) {
    (void) node;
    (void) out;
    (void) name;

    return ENOTDIR;
}

static int demofs_vnode_read_dir(struct vnode* node, struct uio* io) {
    (void) node;
    (void) io;

    return EISDIR;
}

static int demofs_vnode_readdir_file(struct vnode* node, struct uio* io) {
    (void) node;
    (void) io;

    return ENOTDIR;
}


/*
* Now for the actually (somewhat) interesting functions.
*/

static uint8_t demofs_vnode_dirent_type_file(struct vnode* node) {
    (void) node;
    return DT_REG;
}

static uint8_t demofs_vnode_dirent_type_dir(struct vnode* node) {
    (void) node;
    return DT_DIR;
}

static int demofs_vnode_close(struct vnode* node) {
    /*
    * This would need to free any vnode-fs specific information (e.g. current inode)
    * AND ALSO REMOVE IT FROM THE VNODE ARRAY (it is just about to be destroyed - the 
    * reference count is guaranteed to be 0).
    */
    (void) node;
    return 0;
}

static int demofs_vnode_check_open(struct vnode* node, const char* name, int flags) {
    (void) node;

    if (strlen(name) >= MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    if (flags & (O_WRONLY | O_APPEND)) {
        return EROFS;
    }

    return 0;
}

static int demofs_vnode_readdir_dir(struct vnode* node, struct uio* io) {
    (void) node;
    (void) io;

    return ENOSYS;
}

struct vnode* demofs_create_file_vnode();
struct vnode* demofs_create_dir_vnode();

static int demofs_vnode_follow_dir(struct vnode* node, struct vnode** out, const char* name) {
    struct vnode_data* data = node->data;
    ino_t child_inode;
    uint32_t file_length;

    int status = demofs_follow(&data->fs, data->inode, &child_inode, name, &file_length);
    if (status != 0) {
        return status;
    }
    
    /*
    * TODO: don't create if one already exists in the table (which we haven't
    * implemented yet)
    */
   
    struct vnode* child_node;
    if (INODE_IS_DIR(child_inode)) {
        child_node = demofs_create_dir_vnode();
    } else {
        child_node = demofs_create_file_vnode();
    }

    struct vnode_data* child_data = malloc(sizeof(struct vnode_data));
    child_data->inode = child_inode;
    child_data->fs = data->fs;
    child_data->file_length = file_length;

    child_node->data = child_data;

    *out = child_node;

    return 0;
}

static int demofs_vnode_read_file(struct vnode* node, struct uio* io) {
    struct vnode_data* data = node->data;
    assert(data != NULL);
    return demofs_read_file(&data->fs, data->inode, data->file_length, io);
}

static int demofs_vnode_stat(struct vnode* node, struct stat* stat) {
    struct vnode_data* data = node->data;
    assert(data != NULL);

    stat->st_atime = 0;
    stat->st_blksize = 512;
    stat->st_blocks = 0;
    stat->st_ctime = 0;
    stat->st_dev = 0xDEADDEAD;
    stat->st_gid = 0;
    stat->st_ino = data->inode;
    stat->st_mode = (INODE_IS_DIR(data->inode) ? S_IFDIR : S_IFREG) | S_IRWXU | S_IRWXG | S_IRWXO;
    stat->st_mtime = 0;
    stat->st_nlink = 1;
    stat->st_rdev = 0;
    stat->st_size = data->file_length;
    stat->st_uid = 0;

    return 0;
}

const struct vnode_operations demofs_vnode_file_ops = {
    .check_open     = demofs_vnode_check_open,
    .ioctl          = demofs_vnode_ioctl,
    .is_seekable    = demofs_vnode_isseekable,
    .read           = demofs_vnode_read_file,
    .write          = demofs_vnode_write,
    .close          = demofs_vnode_close,
    .truncate       = demofs_vnode_truncate,
    .create         = demofs_vnode_create,
    .follow         = demofs_vnode_follow_file,
    .dirent_type    = demofs_vnode_dirent_type_file,
    .readdir        = demofs_vnode_readdir_file,
    .stat           = demofs_vnode_stat,
};

const struct vnode_operations demofs_vnode_dir_ops = {
    .check_open     = demofs_vnode_check_open,
    .ioctl          = demofs_vnode_ioctl,
    .is_seekable    = demofs_vnode_isseekable,
    .read           = demofs_vnode_read_dir,
    .write          = demofs_vnode_write,
    .close          = demofs_vnode_close,
    .truncate       = demofs_vnode_truncate,
    .create         = demofs_vnode_create,
    .follow         = demofs_vnode_follow_dir,
    .dirent_type    = demofs_vnode_dirent_type_dir,
    .readdir        = demofs_vnode_readdir_dir,
    .stat           = demofs_vnode_stat,
};


int demofs_root_creator(struct vnode* raw_device, struct vnode** out) {
    (void) raw_device;
    
	struct vnode* node = vnode_init(NULL, demofs_vnode_dir_ops);
    struct vnode_data* data = malloc(sizeof(struct vnode_data));
    
    data->fs.disk = raw_device;
    data->fs.root_inode = 9 | (1 << 31);
    data->inode = 9 | (1 << 31);        /* root directory inode */
    data->file_length = 0;              /* root directory has no length */

    node->data = data;

	*out = node;

	return 0;
}

struct vnode* demofs_create_file_vnode()
{
	return vnode_init(NULL, demofs_vnode_file_ops);
}

struct vnode* demofs_create_dir_vnode()
{
	return vnode_init(NULL, demofs_vnode_dir_ops);
}