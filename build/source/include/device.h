#pragma once

#include <common.h>
#include <vfs.h>
#include <interface.h>

void dev_init();
struct vnode* dev_create_vnode(struct std_device_interface* dev) warn_unused;