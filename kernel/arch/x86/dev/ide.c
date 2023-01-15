
#include <machine/ide.h>
#include <machine/portio.h>
#include <machine/interrupt.h>
#include <device.h>
#include <common.h>
#include <assert.h>
#include <errno.h>
#include <heap.h>
#include <thread.h>
#include <vfs.h>
#include <kprintf.h>
#include <synch.h>

/*
* x86/dev/ide.c - IDE/ATA Disk Driver
*
* The disk controller (usually) supports two ATA buses. Each bus can control
* two devices. Hence most systems will support up to four devices.
*
* Each of the two buses uses a different set of IO ports. We *could* detect
* these either manually or using PCI, but we are going to assume they are at
* their 'traditional' locations. They use the IO ports from BASE until BASE+7,
* as well as another port which is used for both device control (when written
* to), or the alternate status register (when read from).
*
* On some rare systems, you might find a third or forth bus, allowing up to
* eight devices. These would probably use ports 0x1E8 and 0x168, (and use
* 0x3E6 and 0x366 for alt. status / device control), but these systems are 
* not common enough for them to be worth supporting.
*/

#define CHANNEL_1_BASE          0x1F0
#define CHANNEL_2_BASE          0x170

#define CHANNEL_1_ALT_STATUS    0x3F6
#define CHANNEL_1_DEV_CTRL      0x3F6

#define CHANNEL_2_ALT_STATUS    0x376
#define CHANNEL_2_DEV_CTRL      0x376

#define SECTOR_SIZE             512


/*
* Used for two purposes:
*
* - Only one read/write operation can happen at any time, we don't want a disk
*   access to be started halfway through another one
*
* - Our system might have multiple hard disks, but two disks share the same
*   I/O ports. Therefore, for simplicity we will only allow one disk to be
*   accessed at a time. (We could make it so two out of the four can be accessed
*   at once if they are using different ports, but we're not going to)
*/
struct semaphore* ide_lock = NULL;

struct ide_data {
    int disk_num;
    uint16_t* transfer_buffer;
};

int ide_check_errors(int disk_num) {
    uint16_t base = disk_num >= 2 ? CHANNEL_2_BASE : CHANNEL_1_BASE;

    uint8_t status = inb(base + 0x7);
    if (status & 0x01) {
        /*
        * Error
        */
        kprintf("IDE ERR 1\n");
        return EIO;

    } else if (status & 0x20) {
        /*
        * Driver write fault
        */
        kprintf("IDE ERR 2\n");
        return EIO;

    } else if (!(status & 0x08)) {
        /*
        * It should be ready for data, but it isn't for some reason.
        */
        kprintf("IDE ERR 3\n");
        return EIO;
    }

    return 0;
}

int ide_poll(int disk_num) {
    uint16_t base = disk_num >= 2 ? CHANNEL_2_BASE : CHANNEL_1_BASE;
    uint16_t alt_status_reg = disk_num >= 2 ? CHANNEL_2_ALT_STATUS : CHANNEL_1_ALT_STATUS;

    /*
    * Delay for a moment by reading the alternate status register.
    */
    for (int i = 0; i < 4; ++i) {
        inb(alt_status_reg);
    }

    /*
    * Wait for the device to not be busy. We have a timeout in case the
    * device is faulty, we don't want to be in an endless loop and freeze
    * the kernel.
    */
    int timeout = 0;
    while (inb(base + 0x7) & 0x80) {
        if (timeout > 99970) {
            /*
            * For the last 30 iterations, wait 10ms
            */
            thread_nano_sleep(10000000);
        }
        if (timeout++ > 100000) {
            return EIO;
        }
    }

    return 0;
}

/*
* Read or write the primary ATA drive on the first controller. We use LBA28, 
* so we are limited to a 28 bit sector number (i.e. disks up to 128GB in size)
*/
static int ide_io(struct std_device_interface* dev, struct uio* io) {
    struct ide_data* data = dev->data;
    int disk_num = data->disk_num;
    
    /*
    * IDE devices do not contain an (accessible) disk buffer in PIO mode, as
    * they transfer data through the IO ports. Hence we must read/write into
    * this buffer first, and then move it safely to the destination. 
    * (we could use DMA instead, but PIO is simpler)
    * 
    * Allow up to 4KB sector sizes. Make sure there is enough room on the
    * stack to handle this.
    */
    uint16_t* buffer = data->transfer_buffer;
    
    assert(dev->block_size <= 4096);    

    /*
    * We can't have a sector size of 0 as it makes no sense and causes division
    * by zero errors.
    */
    assert(dev->block_size != 0);

    int sector = io->offset / dev->block_size;
    int count = io->length_remaining / dev->block_size;

    if (io->offset % dev->block_size != 0) {
        return EINVAL;
    }
    if (io->length_remaining % dev->block_size != 0) {
        return EINVAL;
    }
    if (count <= 0 || sector < 0 || sector > 0xFFFFFFF || (uint64_t) sector + count >= dev->num_blocks) {
        return EINVAL;
    }

    semaphore_acquire(ide_lock);

    uint16_t base = disk_num >= 2 ? CHANNEL_2_BASE : CHANNEL_1_BASE;
    uint16_t dev_ctrl_reg = disk_num >= 2 ? CHANNEL_2_DEV_CTRL : CHANNEL_1_DEV_CTRL;

    while (count > 0) {
        int sectors_in_this_transfer = count < 256 ? count : 255;

        if (io->direction == UIO_WRITE) {
            uio_move(buffer, io, dev->block_size);
        }

        /*
        * Send a whole heap of flags and the high 4 bits of the LBA to the controller.
        * Hardware designs can be weird sometimes.
        */
        outb(base + 0x6, 0xE0 | ((disk_num & 1) << 4) | ((sector >> 24) & 0xF));

        /*
        * Disable interrupts, we are going to use polling.
        */
        outb(dev_ctrl_reg, 2);

        /*
        * May not be needed, but it doesn't hurt to do it.
        */
        outb(base + 0x1, 0x00);

        /*
        * Send the number of sectors, and the sector's LBA.
        */
        outb(base + 0x2, sectors_in_this_transfer);
        outb(base + 0x3, (sector >> 0) & 0xFF);
        outb(base + 0x4, (sector >> 8) & 0xFF);
        outb(base + 0x5, (sector >> 16) & 0xFF);

        /*
        * Send either the read or write command.
        */
        outb(base + 0x7, io->direction == UIO_WRITE ? 0x30 : 0x20);

        /*
        * Wait for the data to be ready.
        */
        ide_poll(disk_num);

        /*
        * Read/write the data from/to the disk using ports.
        */
        if (io->direction == UIO_WRITE) {
            for (int c = 0; c < sectors_in_this_transfer; ++c) {
                if (c != 0) {
                    uio_move(buffer, io, dev->block_size);
                    ide_poll(disk_num);
                }
                
                for (uint64_t i = 0; i < dev->block_size / 2; ++i) {
                    outw(base + 0x00, buffer[i]);
                }
            }

            /*
            * We need to flush the disk's cache if we are writing.
            */
            outb(base + 0x7, 0xE7);
            ide_poll(disk_num);

        } else {
            int err = ide_check_errors(disk_num);
            if (err) {
                semaphore_release(ide_lock);
                return err;
            }
            
            for (int c = 0; c < sectors_in_this_transfer; ++c) {
                if (c != 0) {
                    ide_poll(disk_num);
                }

                for (uint64_t i = 0; i < dev->block_size / 2; ++i) {
                    buffer[i] = inw(base + 0x00);
                }

                uio_move(buffer, io, dev->block_size);
            }
        }

        /*
        * Get ready for the next part of the transfer.
        */
        count -= sectors_in_this_transfer;
        sector += sectors_in_this_transfer;
    }

    semaphore_release(ide_lock);

    return 0;
}


static int ide_get_num_sectors(int disk_num) {
    semaphore_acquire(ide_lock);

    uint16_t base = disk_num >= 2 ? CHANNEL_2_BASE : CHANNEL_1_BASE;

    /*
    * Select the correct drive.
    */
    outb(base + 0x6, 0xE0 | ((disk_num & 1) << 4));

    /*
    * Send the READ NATIVE MAX ADDRESS command, which will return the size
    * of disk in sectors.
    */
    outb(base + 0x7, 0xF8);

    ide_poll(disk_num);

    /*
    * The outputs are in the same registers we use to put the LBA
    * when we read/write from the disk.
    */
    int sectors = 0;
    sectors |= (int) inb(base + 0x3);
    sectors |= ((int) inb(base + 0x4)) << 8;
    sectors |= ((int) inb(base + 0x5)) << 16;
    sectors |= ((int) inb(base + 0x6) & 0xF) << 24;

    semaphore_release(ide_lock);

    return sectors;
}

/*
* We are going to assume an IDE device exists, and we are just going to
* use the first device connected. We are in trouble if a CD drive is the
* first connected device, or if the system uses a SATA hard drive.
*/
void ide_initialise(void) {
    static struct std_device_interface dev;

    if (ide_lock == NULL) {
        ide_lock = semaphore_create(1);
    }

    struct ide_data* data = malloc(sizeof(struct ide_data));
    data->disk_num = 0;
    data->transfer_buffer = malloc(4096);
    dev.data = data;

    dev.termios = NULL;
    dev.block_size = 512;
    dev.num_blocks = ide_get_num_sectors(data->disk_num);
    dev.io = ide_io;
    dev.check_open = interface_check_open_not_needed;
    dev.ioctl = interface_ioctl_not_needed;

    vfs_add_device(&dev, "raw-hd0");
}