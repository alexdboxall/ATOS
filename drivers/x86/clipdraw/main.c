
#include <kprintf.h>
#include <fcntl.h>
#include <vfs.h>
#include <string.h>
#include <heap.h>
#include <vnode.h>
#include <sys/stat.h>

void _driver_entry_point() {
    kprintf("Lol, drivers work\n");
}