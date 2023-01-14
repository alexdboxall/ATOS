#pragma once

#include <vfs.h>
#include <vnode.h>

int demofs_root_creator(struct vnode* raw_device, struct vnode** out);