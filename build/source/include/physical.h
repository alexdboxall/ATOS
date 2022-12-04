#pragma once

/* 
* physical.h - Physical Memory Manager
*/

#include <common.h>

void phys_init(void);
size_t phys_allocate_page(void) warn_unused;
void phys_free_page(size_t phys_addr);