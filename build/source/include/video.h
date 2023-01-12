#pragma once

#include <common.h>
#include <interface.h>

void video_init(void);
int video_register_driver(struct video_device_interface* driver);
int video_putpixel(int x, int y, uint32_t colour);
int video_putline(int x, int y, int width, uint32_t colour);
