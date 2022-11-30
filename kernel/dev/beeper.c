
#include <beeper.h>
#include <errno.h>
#include <spinlock.h>
#include <interface.h>

/*
* dev/beeper.c - Beeper Interface
*
* Wrapper functions for starting and stopping a beeper device.
*/

struct beeper_device_interface* default_beeper_driver = NULL;
struct spinlock beeper_lock;
bool init_lock_yet = false;



int beeper_start(int freq)
{
    if (!init_lock_yet) {
        spinlock_init(&beeper_lock, "beeper lock");
        init_lock_yet = true;
    }

	spinlock_acquire(&beeper_lock);

	if (default_beeper_driver != NULL) {
		default_beeper_driver->start(freq);
		spinlock_release(&beeper_lock);
		return 0;

	} else {
		spinlock_release(&beeper_lock);
		return ENODEV;
	}
}

int beeper_stop(void)
{    
    if (!init_lock_yet) {
        spinlock_init(&beeper_lock, "beeper lock");
        init_lock_yet = true;
    }

	spinlock_acquire(&beeper_lock);

	if (default_beeper_driver != NULL) {
		default_beeper_driver->stop();
		spinlock_release(&beeper_lock);
		return 0;

	} else {
		spinlock_release(&beeper_lock);
		return ENODEV;
	}
}

int beeper_register_driver(struct beeper_device_interface* driver)
{
	if (driver == NULL) {
		return ENODEV;
	}
    
    if (!init_lock_yet) {
        spinlock_init(&beeper_lock, "beeper lock");
        init_lock_yet = true;
    }
	
	spinlock_acquire(&beeper_lock);

	if (default_beeper_driver != NULL) {
		spinlock_release(&beeper_lock);
		return EALREADY;
	
	} else {
		default_beeper_driver = driver;
		spinlock_release(&beeper_lock);
		return 0;
	}
}
