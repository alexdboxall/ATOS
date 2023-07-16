#include <vnode.h>
#include <spinlock.h>
#include <assert.h>
#include <kprintf.h>
#include <synch.h>
#include <heap.h>

/*
* vfs/vnode.c - Virtual Filesystem Nodes
*
* Each vnode represents an abstract file, such as a file, directory or device.
*/

/*
* Allocate and initialise a vnode. The reference count is initialised to 1.
*/
struct vnode* vnode_init(struct std_device_interface* dev, struct vnode_operations ops) {
    struct vnode* node = malloc(sizeof(struct vnode));
    node->dev = dev;
    node->ops = ops;
    node->reference_count = 1;
    node->data = NULL;
    spinlock_init(&node->reference_count_lock, "vnode reference count lock");

    return node;
}

/*
* Cleanup and free an abstract file node.
*/ 
static void vnode_destroy(struct vnode* node) {
    /*
    * The lock can't be held during this process, otherwise the lock will
    * get freed before it is released (which is bad, as we must release it
    * to get interrupts back on).
    */

    assert(node != NULL);
    assert(node->reference_count == 0);

    free(node);
}

/*
* Ensures that a vnode is valid.
*/
void vnode_check(struct vnode* node) {
    /*
    * node->dev is null for filesystems
    */
    assert(node != NULL);
    assert(node->ops.check_open != NULL);
    assert(node->ops.read != NULL);
    assert(node->ops.write != NULL);
    assert(node->ops.ioctl != NULL);
    assert(node->ops.is_seekable != NULL);
    assert(node->ops.is_tty != NULL);
    assert(node->ops.close != NULL);
    assert(node->ops.create != NULL);
    assert(node->ops.stat != NULL);
    assert(node->ops.truncate != NULL);
    assert(node->ops.follow != NULL);
    assert(node->ops.dirent_type != NULL);
    assert(node->ops.readdir != NULL);

    if (spinlock_is_held(&node->reference_count_lock)) {
        assert(node->reference_count > 0);
    } else {
        spinlock_acquire(&node->reference_count_lock);
        assert(node->reference_count > 0);
        spinlock_release(&node->reference_count_lock);
    }
}


/*
* Increments a vnode's reference counter. Used whenever a vnode is 'given' to someone.
*/
void vnode_reference(struct vnode* node) {
    assert(node != NULL);

    spinlock_acquire(&node->reference_count_lock);
    node->reference_count++;
    spinlock_release(&node->reference_count_lock);
}

/*
* Decrements a vnode's reference counter, destorying it if it reaches zero. 
* It should be called to free a vnode 'given' to use when it is no longer needed.
*/
void vnode_dereference(struct vnode* node) {
    vnode_check(node);
    spinlock_acquire(&node->reference_count_lock);

    assert(node->reference_count > 0);
    node->reference_count--;

    if (node->reference_count == 0) {
        vnode_op_close(node);

        /*
        * Must release the lock before we delete it so we can put interrupts back on
        */
        spinlock_release(&node->reference_count_lock);

        vnode_destroy(node);
        return;
    }

    spinlock_release(&node->reference_count_lock);
}


/*
* Wrapper functions for performing operations on a vnode. Also
* performs validation on the vnode.
*/
int vnode_op_check_open(struct vnode* node, const char* name, int flags) {
    vnode_check(node);
    return node->ops.check_open(node, name, flags);
}

int vnode_op_read(struct vnode* node, struct uio* io) {
    vnode_check(node);
    return node->ops.read(node, io);
}

int vnode_op_readdir(struct vnode* node, struct uio* io) {
    vnode_check(node);
    return node->ops.readdir(node, io);
}

int vnode_op_write(struct vnode* node, struct uio* io) {
    vnode_check(node);
    return node->ops.write(node, io);
}

int vnode_op_ioctl(struct vnode* node, int command, void* buffer) {
    vnode_check(node);
    return node->ops.ioctl(node, command, buffer);
}

bool vnode_op_is_seekable(struct vnode* node) {
    vnode_check(node);
    return node->ops.is_seekable(node);
}

int vnode_op_is_tty(struct vnode* node) {
    vnode_check(node);
    return node->ops.is_tty(node);
}

int vnode_op_close(struct vnode* node) {
    /*
    * Don't check for validity as the reference count is currently at zero,
    * (and thus the check will fail), and we just checked its validity in
    * vnode_dereference.
    */
    return node->ops.close(node);
}

int vnode_op_follow(struct vnode* node, struct vnode** new_node, const char* name) {
    vnode_check(node);
    return node->ops.follow(node, new_node, name);
}

uint8_t vnode_op_dirent_type(struct vnode* node) {
    vnode_check(node);
    return node->ops.dirent_type(node);
}

int vnode_op_stat(struct vnode* node, struct stat* stat) {
    vnode_check(node);
    return node->ops.stat(node, stat);
}
