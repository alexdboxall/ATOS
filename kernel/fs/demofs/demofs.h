#pragma once

#include <vfs.h>
#include <vnode.h>

int demofs_root_creator(struct open_file* raw_device, struct open_file** out);