
#include <arch.h>
#include <vfs.h>
#include <heap.h>
#include <fcntl.h>
#include <string.h>
#include <virtual.h>
#include <vnode.h>
#include <sys/stat.h>
#include <kprintf.h>

int load_program(const char* filename, size_t* entry_point, size_t* sbrk_point) {
    struct vnode* file;
    int ret = vfs_open(filename, O_RDONLY, 0, &file);
    if (ret != 0) {
        return ret;
    }

    struct stat st;
    ret = vnode_op_stat(file, &st);
    if (ret != 0) {
        vfs_close(file);
        return ret;
    }
    
    uint8_t* buffer = malloc(st.st_size);
    memset(buffer, 0, st.st_size);

    struct uio uio = uio_construct_kernel_read(buffer, st.st_size, 0);
    ret = vfs_read(file, &uio);
    if (ret != 0) {
        free(buffer);
        vfs_close(file);
        return ret;
    }

    int result = arch_exec(buffer, st.st_size, entry_point, sbrk_point);

    free(buffer);
    vfs_close(file);
    return result;
}

int load_driver(const char* filename, bool lock_in_memory) {
    if (!lock_in_memory) {
        kprintf("Warning: driver %s isn't locked in memory - this could cause crashes, especially on low memory", filename);
    }
    
    struct vnode* file;
    int ret = vfs_open(filename, O_RDONLY, 0, &file);
    if (ret != 0) {
        return ret;
    }

    struct stat st;
    ret = vnode_op_stat(file, &st);
    if (ret != 0) {
        vfs_close(file);
        return ret;
    }
    
    uint8_t* buffer = malloc(st.st_size);
    memset(buffer, 0, st.st_size);

    struct uio uio = uio_construct_kernel_read(buffer, st.st_size, 0);
    ret = vfs_read(file, &uio);
    if (ret != 0) {
        free(buffer);
        vfs_close(file);
        return ret;
    }

    size_t relocation_point = virt_allocate_backed_pages(virt_bytes_to_pages(st.st_size), (lock_in_memory ? VAS_FLAG_LOCKED : 0));
    size_t driver_addr = arch_load_driver(buffer, st.st_size, relocation_point);
    
    free(buffer);
    vfs_close(file);

    // TODO: clean up unused pages

    return arch_start_driver(driver_addr, NULL);
}