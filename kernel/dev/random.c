#include <stdint.h>
#include <random.h>
#include <string.h>
#include <kprintf.h>
#include <heap.h>
#include <errno.h>
#include <vfs.h>
#include <uio.h>
#include <device.h>
#include <fcntl.h>
#include <spinlock.h>

struct spinlock random_lock;

/*
* This is NOT cryptographically secure pseudo-random number generation.
*
* It is good enough for like, shuffling cards for a solitaire game, but not
* for anything involving cryptography.
*/

static uint8_t get_rand(void) {
    static uint64_t next = 1;

    spinlock_acquire(&random_lock);

    next = next * 164603309694725029ULL + 14738995463583502973ULL;
    uint64_t result = (next >> 54) & 0xFF;
    
    spinlock_release(&random_lock);

    return result;
}

int random_io(struct std_device_interface* dev, struct uio* io) {
    (void) dev;

    /*
    * Writing random data doesn't make any sense.
    */
    if (io->direction == UIO_WRITE) {
        return EINVAL;
    }

    while (io->length_remaining > 0) {
        uint8_t random_byte = get_rand();
        int err = uio_move(&random_byte, io, 1);
        if (err) {
            return err;
        }
    }

    return 0;
}

void random_init(void) {
    static struct std_device_interface dev;

    spinlock_init(&random_lock, "random lock");

    dev.data = NULL;
    dev.block_size = 1;
    dev.num_blocks = 0;
    dev.io = random_io;
    dev.check_open = interface_check_open_not_needed;
    dev.ioctl = interface_ioctl_not_needed;

    vfs_add_device(&dev, "rand");
}

uint32_t rand32(void) {
    uint32_t result;

    struct uio io = uio_construct_read(&result, sizeof(result), 0);
    struct vnode* rand;
    
    vfs_open("rand:", O_RDONLY, 0, &rand);
    vfs_read(rand, &io);
    vfs_close(rand);

    return result;
}