
#include <kprintf.h>
#include <fcntl.h>
#include <vfs.h>
#include <string.h>
#include <heap.h>
#include <vnode.h>
#include <sys/stat.h>
#include <video.h>

void _driver_entry_point() {
    video_putrect(0, 0, 100, 100, 0x00FF00);
}