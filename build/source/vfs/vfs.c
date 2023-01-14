
#include <vfs.h>
#include <interface.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <spinlock.h>
#include <panic.h>
#include <vnode.h>
#include <heap.h>
#include <adt.h>
#include <kprintf.h>
#include <device.h>
#include <fcntl.h>
#include <dirent.h>

/*
* vfs/vfs.c - Virtual Filesystem
*
* A standardised system for accessing virtual files, which can either represent a
* file or directory on a filesystem, or a real or virtual device.
*
* This file implements mounting and unmounting of devices and filesystems, opening
* these virtual files through a filepath in the form of DEVICE:PATH/PATH/..., and
* reading, writing and performing other operations on these files.
*
* Filepaths are made of components, separated by a either a colon for the device name,
* or a forward slash otherwise.
*
* The following are absolute filepaths:
* 
*	DEV:PATH			-> absolute path
*	DEV:/PATH			-> absolute path
*
* The following are relative filepaths:
*
*	PATH				-> relative to the current working directory
*	DEV:				-> refers to the device/filesytem itself
*	:PATH				-> relative to the root of current drive
*	:/PATH				-> relative to the root of current drive
*	/PATH				-> relative to the root of current drive
*	:					-> invalid
*
* Trailing slashes are ignored, and repeated slashes are treated as a single slash.
* (i.e. A://B/C//D/// is the same as A:/B/C/D)
*/

/*
* Try not to have non-static functions that return in any way a struct vnode*, as it
* probably means you need to use the reference/dereference functions.
*
*/

/*
* Maximum length of a component of a filepath (e.g. an file/directory's individual name).
*/
#define MAX_COMPONENT_LENGTH	256

/*
* Maximum total length of a path.
*/
#define MAX_PATH_LENGTH			2000

/*
* Maximum number of symbolic links to derefrence in a path before returning ELOOP.
*/
#define MAX_LOOP				5


/*
* A structure for mounted devices and filesystems.
*/
struct mounted_device {
	/* The vnode representing the device / root directory of a filesystem */
	struct vnode* node;

	/* What the device / filesystem mount is called */
	char* name;
};

/*
* A list of all toplevel mounted devices and fileystems.
*/
struct adt_list* mount_list = NULL;

struct spinlock vfs_lock;

void vfs_init(void)
{
	spinlock_init(&vfs_lock, "vfs lock");

	mount_list = adt_list_create();
}

/*
* Checks that a component has a valid filename, i.e. contains no colons, or slashes.
*/ 
static int vfs_check_valid_component_name(const char* name)
{
	assert(name != NULL);
	
	if (name[0] == 0) {
		return EINVAL;
	}

	for (int i = 0; name[i]; ++i) {
		char c = name[i];

		if (c == '/' || c == '\\' || c == ':') {
			return EINVAL;
		}
	}

	return 0;
}

/*
* Checks whether or not a given string refers to a toplevel mounted device or filesystem.
*/
static int vfs_check_device_name_not_used(const char* name)
{
	assert(name != NULL);
	assert(spinlock_is_held(&vfs_lock));

	adt_list_reset(mount_list);
	while (adt_list_has_next(mount_list)) {
		struct mounted_device* device = adt_list_get_next(mount_list);
		if (!strcmp(device->name, name)) {
			return EEXIST;
		}
	}

	return 0;
}

/*
* Given a filepath, and a pointer to an index within that filepath (representing where
* start searching), copies the next component into an output buffer of a given length.
* The index is updated to point to the start of the next component, ready for the next call.
*
* This also handles duplicated and trailing forward slashes.
*/
int vfs_get_path_component(const char* path, int* ptr, char* output_buffer, int max_output_length, char delimiter) {
	int i = 0;
	
	assert(path != NULL);
	assert(ptr != NULL);
	assert(max_output_length >= 1);
	assert(delimiter != 0);
	assert(output_buffer != NULL);
	assert(strlen(path) >= 1);
	assert(*ptr >= 0 && *ptr < (int) strlen(path));

	/*
	* These were meant to be caught at a higher level, so we can apply the current
	* working directory or the current drive.
	*/
	assert(path[0] != '/');
	assert(path[0] != ':');

	output_buffer[0] = 0;

	while (path[*ptr] && path[*ptr] != delimiter) {
		if (i >= max_output_length - 1) {
			return ENAMETOOLONG;
		}

		/*
		* Ensure we always have a null terminated string.
		*/
		output_buffer[i++] = path[*ptr];
		output_buffer[i] = 0;
		(*ptr)++;
	}

	/*
	* Skip past the delimiter (unless we are at the end of the string),
	* as well as any trailing slashes (which could be after a slash delimiter, or
	* after a colon). 
	*/
	if (path[*ptr]) {
		do {
			(*ptr)++;
		} while (path[*ptr] == '/');
	}

	/*
	* Ensure that there are no colons or backslashes in the filename itself.
	*/
	return vfs_check_valid_component_name(output_buffer);
}


/*
* Not at all efficient, but should work.
*/
int vfs_get_final_component(const char* path, char* output_buffer, int max_output_length) {
	
	int path_ptr = 0;

	int status = vfs_get_path_component(path, &path_ptr, output_buffer, max_output_length, ':');
	if (status) {
		return status;
	}

	while (path_ptr < (int) strlen(path)) {
		status = vfs_get_path_component(path, &path_ptr, output_buffer, max_output_length, '/');
		if (status) {
			return status;
		}
	}

	return 0;
}

/*
* Takes in a device name, without the colon, and returns its vnode.
* If no such device is mounted, it returns NULL.
*/
static struct vnode* vfs_get_device_from_name(const char* name) {
	assert(name != NULL);

	/*
	* If this is being called before the VFS has been initialised (i.e. early kprintf),
	* just return with nothing.
	*/
	if (mount_list == NULL) {
		return NULL;
	}

	adt_list_reset(mount_list);
	while (adt_list_has_next(mount_list)) {
		struct mounted_device* device = adt_list_get_next(mount_list);
		if (!strcmp(device->name, name)) {
			return device->node;
		}
	}
	
	return NULL;
}

static void cleanup_vnode_stack(struct adt_stack* stack) {
	/*
	* We need to call dereference on each vnode in the stack before we
	* can call adt_stack_destory.
	*/
	while (adt_stack_size(stack) > 0) {
		struct vnode* node = adt_stack_pop(stack);
		vnode_dereference(node);
	}

	adt_stack_destroy(stack);
}

/*
* Given an absolute filepath, returns the vnode representing
* the file, directory or device. 
*
* Should only be called by vfs_open as the reference count will be incremented.
*/
static int vfs_get_vnode_from_path(const char* path, struct vnode** out, bool want_parent) {
	assert(path != NULL);
	assert(out != NULL);

	if (strlen(path) == 0) {
		return EINVAL;
	}
	if (strlen(path) > MAX_PATH_LENGTH) {
		return ENAMETOOLONG;
	}

	int path_ptr = 0;
	char component_buffer[MAX_COMPONENT_LENGTH];

	struct vnode* current_vnode = NULL;
	int err = vfs_get_path_component(path, &path_ptr, component_buffer, MAX_COMPONENT_LENGTH, ':');
	if (err != 0) {
		return err;
	}

	current_vnode = vfs_get_device_from_name(component_buffer);

	/*
	* No root device found, so we can't continue.
	*/
	if (current_vnode == NULL) {
		return ENODEV;
	}

	/*
	* This will be dereferenced either as we go through the loop, or
	* after a call to vfs_close (this function should only be called 
	* by vfs_open).
	*/
	vnode_reference(current_vnode);

	char component[MAX_COMPONENT_LENGTH + 1];

	/*
	* To go back to a parent directory, we need to keep track of the previous component.
	* As we can go back through many parents, we must keep track of all of them, hence we
	* use a stack to store each vnode we encounter. We will not dereference the vnodes
	* on the stack until the end using cleanup_vnode_stack.
	*/
	struct adt_stack* previous_components = adt_stack_create();
	
	/*
	* Iterate over the rest of the path.
	*/
	while (path_ptr < (int) strlen(path)) {
		if (vnode_op_dirent_type(current_vnode) != DT_DIR) {
			vnode_dereference(current_vnode);
			cleanup_vnode_stack(previous_components);
			return ENOTDIR;
		}

		int status = vfs_get_path_component(path, &path_ptr, component, MAX_COMPONENT_LENGTH, '/');
		if (status != 0) {
			vnode_dereference(current_vnode);
			cleanup_vnode_stack(previous_components);
			return status;
		}

		if (!strcmp(component, ".")) {
			/*
			* This doesn't change where we point to.
			*/
			continue;

		} else if (!strcmp(component, "..")) {
			if (adt_stack_size(previous_components) == 0) {
				/*
				* We have reached the root. Going 'further back' than the root 
				* on Linux just keeps us at the root, so don't do anything here.
				*/
			
			} else {
				/*
				* Pop the previous component and use it.
				*/
				current_vnode = adt_stack_pop(previous_components);
			}

			continue;
		}

		/*
		* Use a seperate pointer so that both inputs don't point to the same
		* location. vnode_follow either increments the reference count or creates
		* a new vnode with a count of one.
		*/
		struct vnode* next_vnode = NULL;
		status = vnode_op_follow(current_vnode, &next_vnode, component);
		if (status != 0) {
			vnode_dereference(current_vnode);
			cleanup_vnode_stack(previous_components);
			return status;
		}	
		
		/*
		* We have a component that can be backtracked to, so add it to the stack.
		* 
		* Also note that vnode_follow adds a reference count, so current_vnode
		* needs to be dereferenced. Conveniently, all components that need to be
		* put on the stack also need dereferencing, and vice versa. 
		*
		* The final vnode we find will not be added to the stack and dereferenced
		* as we won't get here.
		*/
		adt_stack_push(previous_components, current_vnode);
		current_vnode = next_vnode;
	}

	int status = 0;

	if (want_parent) {
		/*
		* Operations that require us to get the parent don't work if we are already
		* at the root.
		*/
		if (adt_stack_size(previous_components) == 0) {
			status = EINVAL;

		} else {
			*out = adt_stack_pop(previous_components);
		}
	} else {
		*out = current_vnode;
	}

	cleanup_vnode_stack(previous_components);
	return status;
}

/*
* TODO: this will need some work.
*/
int vfs_mount_filesystem(const char* filesystem_name, int (*vnode_creator)(struct vnode*, struct vnode**)) {
	char raw_name[MAX_COMPONENT_LENGTH + 8];

	if (strlen(filesystem_name) >= MAX_COMPONENT_LENGTH - 4) {
		return ENAMETOOLONG;
	}

	spinlock_acquire(&vfs_lock);

	strcpy(raw_name, "raw-");
	strcat(raw_name, filesystem_name);

	/*
	* Ensure that the filesystem is not already mounted, but the raw
	* device is.
	*/
	if (vfs_check_device_name_not_used(filesystem_name) != 0) {
		spinlock_release(&vfs_lock);
		return EEXIST;
	}
	if (vfs_check_device_name_not_used(raw_name) == 0) {
		spinlock_release(&vfs_lock);
		return ENODEV;
	}

	/*
	* TODO: open / reference count this so you can't unmount a raw disk while
	*       it has a filesystem mounted
	*/
	struct vnode* raw_device = vfs_get_device_from_name(raw_name);

	struct vnode* root_directory;
	int status = vnode_creator(raw_device, &root_directory);
	if (status != 0) {
		spinlock_release(&vfs_lock);
		return status;
	}

	struct fs* filesystem = malloc(sizeof(struct fs));
	filesystem->raw_device = raw_device;
	filesystem->root_directory = root_directory;

	struct mounted_device* mount = malloc(sizeof(struct mounted_device));
	mount->name = strdup(filesystem_name);
	mount->node = root_directory;

	adt_list_add_back(mount_list, mount);

	spinlock_release(&vfs_lock);

	return 0;
}

/*
* Mounts a directory as a root mount (e.g. mapping hd0:/System to sys:)
*/
int vfs_add_virtual_mount_point(const char* mount_name, const char* filename) {
    if (mount_name == NULL) {
        return EINVAL;
    }
    
    if (strlen(mount_name) >= MAX_COMPONENT_LENGTH) {
        return ENAMETOOLONG;
    }

    int status = vfs_check_valid_component_name(mount_name);
	if (status != 0) {
		return status;
	}

    struct vnode* file;
    status = vfs_open(filename, O_RDONLY, 0, &file);
    if (status != 0) {
        return status;
    }

    uint8_t type = vnode_op_dirent_type(file);
    if (type != DT_DIR) {
        return ENOTDIR;
    }

	spinlock_acquire(&vfs_lock);

    /*
	* We must acquire the spinlock before doing this, in case someone
	* else adds (or removes) a device while we weren't looking.
	*/
	if (vfs_check_device_name_not_used(mount_name) != 0) {
		spinlock_release(&vfs_lock);
		return EEXIST;
	}

	struct mounted_device* mount = malloc(sizeof(struct mounted_device));
	mount->name = strdup(mount_name);
	mount->node = file;

	adt_list_add_back(mount_list, mount);
	
	spinlock_release(&vfs_lock);

	return 0;
}

/*
* Add a device/filesystem as a root mount point in the virtual file system. 
*/
int vfs_add_device(struct std_device_interface* dev, const char* name)
{
	if (name == NULL || dev == NULL) {
		return EINVAL;
	}

	if (strlen(name) >= MAX_COMPONENT_LENGTH) {
		return ENAMETOOLONG;
	}

	int status = vfs_check_valid_component_name(name);
	if (status != 0) {
		return status;
	}

	spinlock_acquire(&vfs_lock);
	
	/*
	* We must acquire the spinlock before doing this, in case someone
	* else adds (or removes) a device while we weren't looking.
	*/
	if (vfs_check_device_name_not_used(name) != 0) {
		spinlock_release(&vfs_lock);
		return EEXIST;
	}

	struct mounted_device* mount = malloc(sizeof(struct mounted_device));
	mount->name = strdup(name);
	mount->node = dev_create_vnode(dev);

	adt_list_add_back(mount_list, mount);
	
	spinlock_release(&vfs_lock);

	return 0;
}


/*
* Removes a toplevel device/filesystem from the tree. The vnode may still be used by
* other processes if they hold a reference to it.
*/
int vfs_remove_device(const char* name) {
	if (name == NULL) {
		return EINVAL;
	}

	if (vfs_check_valid_component_name(name) != 0) {
		return EINVAL;
	}

	spinlock_acquire(&vfs_lock);

	/*
	* Scan through the mount table for the device
	*/ 
	adt_list_reset(mount_list);
	
	while (adt_list_has_next(mount_list)) {
		struct mounted_device* mount = adt_list_get_next(mount_list);

		if (!strcmp(mount->name, name)) {
			/*
			* Decrement the reference that was initially created way back in
			* vfs_add_device in the call to dev_create_vnode.
			*/
			vnode_dereference(mount->node);

			adt_list_remove_element(mount_list, mount);
			free(mount->name);

			spinlock_release(&vfs_lock);

			return 0;
		}
	}

	spinlock_release(&vfs_lock);

	return ENODEV;
}

int vfs_close(struct vnode* node) {
	if (node == NULL) {
		return EINVAL;
	}

    vnode_dereference(node);
	return 0;
}


/*
* Open a file from a path. Currently only accepts absolute paths.
*/
int vfs_open(const char* path, int flags, mode_t mode, struct vnode** out) {
	if (path == NULL || out == NULL || strlen(path) <= 0) {
		return EINVAL;
	}

    /*
    * O_NONBLOCK has two meanings, one of which is to prevent open() from 
    * blocking for a "long time" to open (e.g. serial ports). This usage of the flags
    * does not get saved. As we don't block for a "long time" here, we can ignore
    * this usage of the flag. We also don't want O_NONBLOCK to ever be saved to initial_flags,
    * as that is where the second meaning gets used.
    */
    if (flags & O_NONBLOCK) {
        flags &= ~O_NONBLOCK;
    }

	int status;

	/*
	* Find the last component in the path
	*/
	char name[MAX_COMPONENT_LENGTH + 1];
	status = vfs_get_final_component(path, name, MAX_COMPONENT_LENGTH);
	if (status) {
		return status;
	}

	/*
	* Grab the vnode from the path.
	*/
	struct vnode* node;

    /*
    * Lookup a (hopefully) existing file.
    */
    status = vfs_get_vnode_from_path(path, &node, false);
    if (status == ENOENT && (flags & O_CREAT)) {
        /*
		* Process O_CREAT and O_EXCL, set node
		* TODO: call vnode_create(parent, ..., is_excl, mode)
		*/
	
		(void) mode;

		/*
		* Get the parent folder.
		*/
		status = vfs_get_vnode_from_path(path, &node, true);
		if (status) {
			return status;
		}

		/*
		status = vnode_create(..., name);
		vnode_dereference(parent);
		if (status) {
			return status;
		}*/

    } else if (status != 0) {
        return status;
    }

	status = vnode_op_check_open(node, name, flags & O_ACCMODE);
	if (status) {
		vnode_dereference(node);
		return status;
	}

	node->can_read = (flags & O_ACCMODE) != O_WRONLY;
	node->can_write = (flags & O_ACCMODE) != O_RDONLY;

	if (vnode_op_dirent_type(node) == DT_DIR && node->can_write) {
		/*
		* You cannot write to a directory. This also prevents truncation.
		*/
		vnode_dereference(node);
		return EISDIR;
	}
	
	if (flags & O_TRUNC) {
		if (node->can_write) {
			// TODO: status = vnode_truncate(node)
			// if (status) { return status; }
		} else {
			return EINVAL;
		}
	}

    node->initial_mode = mode;
    node->flags = flags;

	*out = node;
	return 0;
}

/*
* Write data to a file.
*/
int vfs_write(struct vnode* node, struct uio* io) {
	if (io == NULL || io->address == NULL) {
		return EINVAL;
	}
    if (!node->can_write) {
        return EBADF;
    }
    // TODO: lock??

    if (node->flags & O_APPEND) {
        // TODO: seek the end of the file
    }
	return vnode_op_write(node, io);
}

/*
* Read data from the file.
*/
int vfs_read(struct vnode* node, struct uio* io) {
	if (io == NULL || io->address == NULL) {
		return EINVAL;
	}
    if (!node->can_read) {
        return EBADF;
    }

	if (vnode_op_dirent_type(node) == DT_DIR) {
		return EISDIR;
	}

	return vnode_op_read(node, io);
}