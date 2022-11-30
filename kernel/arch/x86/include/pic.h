#pragma once

#include <common.h>

#define PIC_IRQ_BASE    32

void pic_initialise(void);
void pic_eoi(int num);
bool pic_is_spurious(int num);