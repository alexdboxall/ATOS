
#include <machine/floppy.h>
#include <machine/portio.h>
#include <machine/interrupt.h>
#include <machine/pic.h>
#include <machine/pit.h>
#include <device.h>
#include <cpu.h>
#include <common.h>
#include <assert.h>
#include <errno.h>
#include <heap.h>
#include <string.h>
#include <thread.h>
#include <vfs.h>
#include <kprintf.h>
#include <synch.h>
#include <virtual.h>

/*
* x86/dev/floppy.c - Floppy Disk Driver
*
* Floppy drives are very shit, but it's the only way I can test on real hardware
* at the moment, because I'm not installing this on my main computer, and my only other
* computer with me at the moment is a laptop from 1996 that doesn't support USB boot.
*
* This is a 'quick-and-dirty' read-only floppy driver. It doesn't support locking,
* and does barely any error correction.
*/

uint8_t* cylinder_buffer;
uint8_t* cylinder_zero;
int stored_cylinder = -1;
bool got_cylinder_zero = false;

#define FLOPPY_DOR      2
#define FLOPPY_MSR      4
#define FLOPPY_FIFO     5
#define FLOPPY_CCR      7

#define CMD_SPECIFY     0x03
#define CMD_READ        0x06
#define CMD_RECALIBRATE 0x07
#define CMD_SENSE_INT   0x08
#define CMD_SEEK        0x0F
#define CMD_CONFIGURE   0x13

static void floppy_write_cmd(int base, int cmd) {
    /*
    * Write a command to the floppy drive. We need to poll until
    * the drive is ready for us, of course with a timeout in case something
    * goes wrong (which is pretty likely).
    */

    for (int i = 0; i < 60; ++i) {
        thread_nano_sleep(10000000);
        if (inb(base + FLOPPY_MSR) & 0x80) {
            outb(base + FLOPPY_FIFO, cmd);
            return;
        }
    }

    kprintf("floppy_write_cmd: timeout\n");
}

static uint8_t floppy_read_data(int base) {
    /*
    * Read data from the floppy drive.
    */
   for (int i = 0; i < 60; ++i) {
        thread_nano_sleep(10000000);
        if (inb(base + FLOPPY_MSR) & 0x80) {
            return inb(base + FLOPPY_FIFO);
        }
    }

    kprintf("floppy_read_data: timeout\n");
    return 0;
}

static void floppy_check_interrupt(int base, int* st0, int* cyl) {
    /*
    * After some commands, we must read status bytes before the drive
    * will accept any other commands.
    */

    floppy_write_cmd(base, CMD_SENSE_INT);

    *st0 = floppy_read_data(base);
    *cyl = floppy_read_data(base);
}

/*
* We want to to keep the drive on for a second or so after a command so we
* don't need to keep turning it on and off for every command.
*
* The state can be 0 (off), 1 (on) or 2 (currently on, but will shortly be turned off).
*/

static volatile int floppy_motor_state = 0;
static volatile int floppy_motor_ticks = 0;

/*
* Start or stop using the floppy motor. The motor will stay on for a few moments after
* turning off via this function.
*/
static void floppy_motor(int base, bool state) {
    if (state) {
        /*
        * Turn on the motor if it isn't already on, and put it into the on
        * state.
        */
        if (!floppy_motor_state) {
            outb(base + FLOPPY_DOR, 0x1C);
            thread_nano_sleep(150000000);
        }
        floppy_motor_state = 1;

    } else {
        /*
        * Put it into the 'waiting to be turned off' state
        */
        floppy_motor_state = 2;
        floppy_motor_ticks = 1000;
    }
}

static volatile bool floppy_got_irq = false;

static void floppy_irq_wait() {
    /*
    * Wait for the interrupt to come. If it doesn't the system will probably
    * lockup.
    */
    while (!floppy_got_irq) {
        thread_nano_sleep(10000000);
    }

    /*
    * Clear it for next time.
    */
    floppy_got_irq = false;
}

static int floppy_irq_handler(struct x86_regs* r) {
    (void) r;

    floppy_got_irq = true;
    return 0;
}

static void floppy_timer(void* arg) {
    (void) arg;

    /*
    * This runs in its own thread, and will turn off the floppy
    * after it hasn't been used for 300ms.
    */

    while (1) {
        thread_nano_sleep(50000000);
        if (floppy_motor_state == 2) {
            floppy_motor_ticks -= 50;
            if (floppy_motor_ticks <= 0) {
                /*
                * Actually turn off the motor.
                */
                outb(0x3F0 + FLOPPY_DOR, 0x0C);
                floppy_motor_state = 0;
            }
        }
    }
}

/*
* Move to cylinder 0.
*/
static int floppy_calibrate(int base) {
    int st0 = -1;
    int cyl = -1;

    floppy_motor(base, true);

    /*
    * Try up to 10 times, as floppies can be rather unreliable.
    */
    for (int i = 0; i < 10; ++i) {
        floppy_write_cmd(base, CMD_RECALIBRATE);
        floppy_write_cmd(base, 0);

        floppy_irq_wait();
        floppy_check_interrupt(base, &st0, &cyl);

        if (st0 & 0xC0) {
            continue;
        }

        if (cyl == 0) {
            floppy_motor(base, false);
            return 0;
        }
    }

    kprintf("couldn't calibrate floppy\n");
    floppy_motor(base, false);
    return EIO;
}


static void floppy_configure(int base) {
    floppy_write_cmd(base, CMD_CONFIGURE);
    floppy_write_cmd(base, 0x00);
    floppy_write_cmd(base, 0x08);
    floppy_write_cmd(base, 0x00);
}

/*
* Reset the floppy disk controller. The only way to recover from some errors.
*/
static int floppy_reset(int base) {
    outb(base + FLOPPY_DOR, 0x00);
    outb(base + FLOPPY_DOR, 0x0C);

    floppy_irq_wait();

    for (int i = 0; i < 4; ++i) {
        int st0, cyl;
        floppy_check_interrupt(base, &st0, &cyl);
    }

    outb(base + FLOPPY_CCR, 0x00);

    floppy_motor(base, true);

    floppy_write_cmd(base, CMD_SPECIFY);
    floppy_write_cmd(base, 0xDF);
    floppy_write_cmd(base, 0x02);

    thread_nano_sleep(300000000);

    floppy_configure(base);

    thread_nano_sleep(300000000);

    floppy_motor(base, false);

    return floppy_calibrate(base);
}

/*
* Move to a given cylinder.
*/
static int floppy_seek(int base, int cylinder, int head) {
    int st0, cyl;
    floppy_motor(base, true);

    /*
    * Again, try up to 10 times.
    */
    for (int i = 0; i < 10; ++i) {
        floppy_write_cmd(base, CMD_SEEK);
        floppy_write_cmd(base, head << 2);
        floppy_write_cmd(base, cylinder);

        floppy_irq_wait();
        floppy_check_interrupt(base, &st0, &cyl);

        if (st0 & 0xC0) {
            continue;
        }

        if (cyl == cylinder) {
            floppy_motor(base, false);
            return 0;
        }
    }

    kprintf("couldn't seek floppy\n");
    floppy_motor(base, false);
    return EIO;
}

/*
* Floppies use DMA (direct memory access) to move data from the disk to memory.
* We must setup the DMA controller ready for the transfer.
*/
static void floppy_dma_init(void) {
    /*
    * Put the data at *physical address* 0x10000. The address can be anywhere 
    * under 24MB that doesn't cross a 64KB boundary. We choose this location as 
    * it should be unused as this is where the temporary copy of the kernel was
    * stored during boot.
    */
    uint32_t addr = (uint32_t) 0x10000;

    /*
    * We must give the DMA the actual count minus 1.
    */
    int count = 0x4800 - 1;

    /*
    * Send some magical stuff to the DMA controller.
    */
    outb(0x0A, 0x06);
    outb(0x0C, 0xFF);
    outb(0x04, (addr >> 0) & 0xFF);
    outb(0x04, (addr >> 8) & 0xFF);
    outb(0x81, (addr >> 16) & 0xFF);
    outb(0x0C, 0xFF);
    outb(0x05, (count >> 0) & 0xFF);
    outb(0x05, (count >> 8) & 0xFF);
    outb(0x0B, 0x46);
    outb(0x0A, 0x02);
}

/*
* Read an entire cylinder (both heads) into memory.
*/
static int floppy_do_cylinder(int base, int cylinder) {
    /*
    * Move both heads to the correct cylinder.
    */
    if (floppy_seek(base, cylinder, 0) != 0) return EIO;
    if (floppy_seek(base, cylinder, 1) != 0) return EIO;

    /*
    * This time, we'll try up to 20 times.
    */
    for (int i = 0; i < 20; ++i) {
        floppy_motor(base, true);

        if (i % 5 == 4) {
            if (i % 10 == 8) {
                floppy_reset(base);
            }

            floppy_calibrate(base);

            if (floppy_seek(base, cylinder, 0) != 0) return EIO;
            if (floppy_seek(base, cylinder, 1) != 0) return EIO;
        }

        floppy_dma_init();

        thread_nano_sleep(10000000);

        /*
        * Send the read command.
        */
        floppy_write_cmd(base, CMD_READ | 0xC0);
        floppy_write_cmd(base, 0);
        floppy_write_cmd(base, cylinder);
        floppy_write_cmd(base, 0);
        floppy_write_cmd(base, 1);
        floppy_write_cmd(base, 2);
        floppy_write_cmd(base, 18);
        floppy_write_cmd(base, 0x1B);
        floppy_write_cmd(base, 0xFF);

        floppy_irq_wait();

        /*
        * Read back some status information, some of which is very mysterious.
        */
        uint8_t st0, st1, st2, rcy, rhe, rse, bps;
        st0 = floppy_read_data(base);
        st1 = floppy_read_data(base);
        st2 = floppy_read_data(base);
        rcy = floppy_read_data(base);
        rhe = floppy_read_data(base);
        rse = floppy_read_data(base);
        bps = floppy_read_data(base);

        /*
        * Check for errors. More tests can be done, but it would make the code
        * even longer.
        */
        if (st0 & 0xC0) {
            continue;
        }
        if (st1 & 0x80) {
            continue;
        }
        if (st0 & 0x8) {
            continue;
        }
        if (st1 & 0x20) {
            continue;
        }
        if (st1 & 0x10) {
            continue;
        }
        if (bps != 2) {
            continue;
        }

        (void) st2;
        (void) rcy;
        (void) rhe;
        (void) rse;

        floppy_motor(base, false);
        return 0;
    }

    kprintf("couldn't read floppy\n");
    floppy_motor(base, false);
    return EIO;
}

static int floppy_io(struct std_device_interface* dev, struct uio* io) {
    /*
    * DemoFS is readonly so there is no need to support writing. If you do want
    * to implement writing to a floppy, you need to:
    *       - send CMD_WRITE (0x05) instead of CMD_READ (0x06)
    *       - use 0x4A instead of 0x46 in floppy_dma_init()
    *       - copy the data from uio to the DMA buffer before sending the command,
    *         instead of from DMA buffer to uio afterwards
    *       - remove or completely rewrite the caching
    * 
    * Writing entire cylinders may not be the best choice (speed-wise) either.
    */
    if (io->direction == UIO_WRITE) {
        return ENOSYS;
    }

    assert(dev->block_size == 512);

    int lba = io->offset / dev->block_size;
    int count = io->length_remaining / dev->block_size;

    if (io->offset % dev->block_size != 0) {
        return EINVAL;
    }
    if (io->length_remaining % dev->block_size != 0) {
        return EINVAL;
    }
    if (count <= 0 || count > 0xFF || lba < 0 || lba > 0xFFFFFFF || (uint64_t) lba >= dev->num_blocks) {
        return EINVAL;
    }

next_sector:;
    /*
    * Floppies use CHS (cylinder, head, sector) for addressing sectors instead of 
    * LBA (linear block addressing). Hence we need to convert to CHS.
    * 
    * Head: which side of the disk it is on (i.e. either the top or bottom)
    * Cylinder: which 'slice' (cylinder) of the disk we should look at
    * Sector: which 'ring' (sector) of that cylinder we should look at
    * 
    * Note that sector is 1-based, whereas cylinder and head are 0-based.
    * Don't ask why.
    */
    int head = (lba % (18 * 2)) / 18;
    int cylinder = (lba / (18 * 2));
    int sector = (lba % 18) + 1;

    /*
    * Cylinder 0 has some commonly used data, so cache it seperately for improved
    * speed.
    */
    if (cylinder == 0) {
        if (!got_cylinder_zero) {
            got_cylinder_zero = true;

            int error = floppy_do_cylinder(0x3F0, cylinder);

            if (error != 0) {
                return error;
            }

            memcpy(cylinder_zero, cylinder_buffer, 0x4800);

            /* 
            * Must do this as we trashed the cached cylinder.
            * Luckily we only ever need to do this once.
            */
            stored_cylinder = cylinder;
        }

        uio_move(cylinder_zero + (512 * (sector - 1 + head * 18)), io, dev->block_size);

    } else {
        if (cylinder != stored_cylinder) {
            int error = floppy_do_cylinder(0x3F0, cylinder);

            if (error != 0) {
                return error;
            }
        }

        uio_move(cylinder_buffer + (512 * (sector - 1 + head * 18)), io, dev->block_size);
        stored_cylinder = cylinder;
    }
    
    /*
    * Read only read one sector, so we need to repeat the process if multiple sectors
    * were requested. We could just do a larger copy above, but this is a bit simpler,
    * as we don't need to worry about whether the entire request is on cylinder or not.
    */
    if (count > 0) {
        ++lba;
        --count;
        goto next_sector;
    }

    return 0;
}

void floppy_initialise(void) {
    static struct std_device_interface dev;

    cylinder_zero = malloc(0x4800);

    dev.data = NULL;
    dev.block_size = 512;
    dev.num_blocks = 2880;
    dev.io = floppy_io;
    dev.check_open = interface_check_open_not_needed;
    dev.ioctl = interface_ioctl_not_needed;
    dev.is_tty = 0;

    vfs_add_device(&dev, "raw-hd0");

    x86_register_interrupt_handler(PIC_IRQ_BASE + 6, floppy_irq_handler);

    cylinder_buffer = (uint8_t*) virt_allocate_unbacked_krnl_region(0x4800);
    for (int i = 0; i < 5; ++i) {
        vas_map(current_cpu->current_vas, 0x10000 + i * 4096, (size_t) cylinder_buffer + i * 4096, VAS_FLAG_LOCKED | VAS_FLAG_WRITABLE);
    }
    
    thread_create(floppy_timer, NULL, current_cpu->current_vas);
    floppy_reset(0x3F0);
}