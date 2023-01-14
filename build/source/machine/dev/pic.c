
#include <kprintf.h>
#include <machine/pic.h>
#include <machine/portio.h>

/*
* x86/dev/pic.c - Programmable Interrupt Controller
*
* The PIC controlls the hardare raised interrupts (IRQs), i.e. those from 
* 'external' devices such as the keyboard, system timer, disk, etc. Hence the
* PIC must be configured before any external interrupts can be seen by the CPU.
*
* There are two quirks to certain IRQs. The first comes about by the fact that 
* each system actually has two PICs - the primary PIC handling IRQs 0-7, and the
* secondary PIC handling IRQs 8-15. They are connected (cascaded) to each other, 
* using IRQ 2 for communication. Thus, an IRQ 2 will get to the CPU.
*
* The other quirk is the 'spurious interrupt', which can occur on IRQ 7 or 15. 
* See pic_is_spurious for more details.
*/

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define PIC_EOI         0x20
#define PIC_REG_ISR     0x0B

#define ICW1_ICW4       0x01
#define ICW1_INIT       0x10
#define ICW4_8086       0x01


/*
* Delay for a short period of time, for use in betwwen IO calls to the PIC.
* This is required as some PICs have a hard time keeping up with the speed 
* of modern CPUs (the original PIC was introduced in 1976!).
*/
static void pic_io_wait(void) {
    
}

/*
* Read an internal PIC register.
*/
static uint16_t pic_read_reg(int ocw3) {
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return ((uint16_t) inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/*
* Due to a race condition between the PIC and the CPU, we sometimes get a
* 'spurious' interrupt sent to the CPU on IRQ 7 or 15. If an IRQ 7 or 15
* arrives, we need to check if it an actual interrupt or a spurious interrupt.
* Distinguishing them is important - spurious IRQs may cause drivers to misbehave,
* and we don't need to send an EOI after a spurious interrupt.
*/
bool pic_is_spurious(int num) {
    if (num == PIC_IRQ_BASE + 7) {
        uint16_t isr = pic_read_reg(PIC_REG_ISR);
        if (!(isr & (1 << 7))) {
            return true;
        }

    } else if (num == PIC_IRQ_BASE + 15) {
        uint16_t isr = pic_read_reg(PIC_REG_ISR);
        if (!(isr & (1 << 15))) {
            /*
            * It is spurious, but the primary PIC doesn't know that, as it came
            * from the secondary PIC. So only send an EOI to the primary PIC.
            */
            outb(PIC1_COMMAND, PIC_EOI);
            return true;
        }
    }
    
    return false;
}

/*
* Acknowledge the previous interrupt. We will not receive any interrupts of
* the same type until we have acknowledged it.
*/
void pic_eoi(int num) {
    if (num >= PIC_IRQ_BASE + 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    outb(PIC1_COMMAND, PIC_EOI);
}

/*
* Change which interrupt numbers are used by the IRQs. They will initially
* use interrupts 0 through 15, which isn't very good as it conflicts with the
* interrupt numbers for the CPU exceptions. 
*/
static void pic_remap(int offset) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    pic_io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    pic_io_wait();
    outb(PIC1_DATA, offset);
    pic_io_wait();
    outb(PIC2_DATA, offset + 8);
    pic_io_wait();
    outb(PIC1_DATA, 4);
    pic_io_wait();
    outb(PIC2_DATA, 2);
    pic_io_wait();

    outb(PIC1_DATA, ICW4_8086);
    pic_io_wait();
    outb(PIC2_DATA, ICW4_8086);
    pic_io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/*
* Initialise the PIC.
*/
void pic_initialise(void) {
    /*
    * Remap the PIC so that interrupts 0-15 are mapped to 32-47.
    * This way we don't have conflicts with the CPU exceptions which are
    * hard-wired to these interrupt numbers.
    */
    pic_remap(PIC_IRQ_BASE);

    /*
    * Now we can disable all masks, allowing interrupts to reach the CPU.
    */
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);
}