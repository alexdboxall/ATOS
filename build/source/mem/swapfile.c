
#include <virtual.h>
#include <physical.h>
#include <arch.h>
#include <vfs.h>
#include <fcntl.h>
#include <kprintf.h>
#include <string.h>
#include <heap.h>
#include <panic.h>
#include <uio.h>
#include <bitarray.h>

#define SWAPFILE_MAX_SIZE_BYTES (1024 * 1024 * 16)
#define SWAPFILE_MAX_PAGES      (SWAPFILE_MAX_SIZE_BYTES / ARCH_PAGE_SIZE)

static size_t swapfile_initial_sector = 0;
static size_t swapfile_sectors_per_page = 0;
static size_t swapfile_sector_size = 0;
static struct open_file* swapfile_drive;
static uint8_t* swapfile_usage_bitmap;
static struct spinlock swapfile_lock;

void swapfile_init() {
    swapfile_initial_sector = 1440 * 2;
    swapfile_sector_size = 512;
    swapfile_sectors_per_page = (ARCH_PAGE_SIZE + swapfile_sector_size - 1) / swapfile_sector_size;    

    int status = vfs_open("raw-hd0", O_RDWR, 0, &swapfile_drive);  
    if (status != 0) {
        panic("swapfile: failed to open");
    }

    swapfile_usage_bitmap = malloc(SWAPFILE_MAX_PAGES / 8);
    memset(swapfile_usage_bitmap, 0, SWAPFILE_MAX_PAGES / 8);

    spinlock_init(&swapfile_lock, "swapfile lock");
}

/*
* Writes a page's worth of data to the disk, and returns an id
* which can be used to retrieve it later on.
*/
size_t swapfile_write(uint8_t* data) {
    spinlock_acquire(&swapfile_lock);

    size_t id = SWAPFILE_MAX_PAGES;
    for (size_t i = 0; i < SWAPFILE_MAX_PAGES; ++i) {
        if (!bitarray_is_set(swapfile_usage_bitmap, i)) {
            bitarray_set(swapfile_usage_bitmap, i);
            id = i;
            break;
        }
    }

    if (id == SWAPFILE_MAX_PAGES) {
        panic("swapfile: out of space");
    }

    struct uio io = uio_construct_kernel_write(data, ARCH_PAGE_SIZE, (swapfile_initial_sector + id * swapfile_sectors_per_page) * swapfile_sector_size);

    int status = vfs_write(swapfile_drive, &io);
    if (status != 0) {
        panic("swapfile: failed to write");
    }

    memset(data, 0, ARCH_PAGE_SIZE);

    spinlock_release(&swapfile_lock);

    return id;
}


/*
* Reloads a page from the disk and saves it into RAM at a given location.
*/
void swapfile_read(uint8_t* data, size_t id) {
    if (id >= SWAPFILE_MAX_PAGES) {
        panic("page fault in non-paged area");
    }

    struct uio io = uio_construct_kernel_read(data, ARCH_PAGE_SIZE, (swapfile_initial_sector + id * swapfile_sectors_per_page) * swapfile_sector_size);

    int status = vfs_read(swapfile_drive, &io);
    if (status != 0) {
        panic("swapfile: failed to read");
    }

    bitarray_clear(swapfile_usage_bitmap, id);
}
