#pragma once

#define ARCH_PAGE_SIZE	        4096
#undef ARCH_STACK_GROWS_UPWARD 
#define ARCH_STACK_GROWS_DOWNWARD

/*
* Non-inclusive of ARCH_USER_AREA_LIMIT
*/
#define ARCH_USER_AREA_BASE     0x08000000
#define ARCH_USER_AREA_LIMIT    0xC0000000


#define ARCH_USER_STACK_BASE    0x08000000
#define ARCH_USER_STACK_LIMIT   0x10000000


/*
* Non-inclusive of ARCH_KRNL_SBRK_LIMIT. Note that we can't use the top 8MB,
* as we use that for recursive mapping.
*/
#define ARCH_KRNL_SBRK_BASE     0xC8000000
#define ARCH_KRNL_SBRK_LIMIT    0xFF800000

#define ARCH_MAX_CPU_ALLOWED    16
