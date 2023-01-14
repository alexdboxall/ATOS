

#include <video.h>
#include <errno.h>
#include <spinlock.h>
#include <interface.h>

/*
* dev/video.c - Videa Interface
*
* Wrapper functions for manipulating a video device.
*/

struct video_device_interface* default_video_driver = NULL;
struct spinlock video_lock;

void video_init(void) {
    spinlock_init(&video_lock, "video lock");
}

int video_register_driver(struct video_device_interface* driver)
{
	if (driver == NULL) {
		return ENODEV;
	}
    
	spinlock_acquire(&video_lock);

	if (default_video_driver != NULL) {
		spinlock_release(&video_lock);
		return EALREADY;
	
	} else {
		default_video_driver = driver;
		spinlock_release(&video_lock);
		return 0;
	}
}

int video_putpixel(int x, int y, uint32_t colour) {
	spinlock_acquire(&video_lock);

	if (default_video_driver != NULL) {
		default_video_driver->putpixel(default_video_driver, x, y, colour);
		spinlock_release(&video_lock);
		return 0;

	} else {
		spinlock_release(&video_lock);
		return ENODEV;
	}
}

int video_putline(int x, int y, int width, uint32_t colour) {
    spinlock_acquire(&video_lock);

	if (default_video_driver != NULL) {
        default_video_driver->putline(default_video_driver, x, y, width, colour);
		spinlock_release(&video_lock);
		return 0;

	} else {
		spinlock_release(&video_lock);
		return ENODEV;
	}
}

int video_putrect(int x, int y, int width, int height, uint32_t colour) {
    spinlock_acquire(&video_lock);

	if (default_video_driver != NULL) {
        default_video_driver->putrect(default_video_driver, x, y, width, height, colour);
		spinlock_release(&video_lock);
		return 0;

	} else {
		spinlock_release(&video_lock);
		return ENODEV;
	}
}