#pragma once

#include <interface.h>

int beeper_start(int freq);
int beeper_stop(void);
int beeper_register_driver(struct beeper_device_interface* driver);
