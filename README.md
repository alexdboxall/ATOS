## ATOS - Operating System Kernel ##

ATOS is an operating system inspired by OS/161. It is written in C, and is designed easy to understand, and lightweight (unlike [my previous OS...](https://github.com/alexdboxall/Banana-Operating-System)). 

For example, ATOS it only requires 3MB of RAM to run on an x86, and is only around 15000 lines of commented code (if you exclude the ACPICA driver). 

It is currently only implemented for x86, but should be easy to port to other platforms (via the arch/ folder, and arch.h).

To run it in QEMU, use the following command: `qemu-system-i386 -soundhw pcspk -hda build/output/disk.bin -m 8`

![ATOS Kernel](https://github.com/alexdboxall/ATOS/blob/main/doc/img1.png "ATOS Kernel")

It supports the following features:
- Boots and runs on real hardware
- A virtual filesystem (VFS) to manage files, folders and devices
- Dynamic loading of kernel drivers from ELF files (e.g. video and keyboard drivers)
- Virtual memory manager which supports page replacement when it runs out of physical memory, allocate on demand pages, and copy on write pages
- Userspace programs loaded from the disk
- A very basic read-only custom filesystem (DemoFS)
- PS/2 keyboard, IDE and floppy drives

The TODO list: 
- Read/write FAT12/16/32 driver
- Making use of the ACPICA driver
- Disk auto-detection
- Disk caching
- More system calls
- PCI / AHCI disk drive support
- Fixing up all the other TODOs in the code!!

![ATOS Kernel](https://github.com/alexdboxall/ATOS/blob/main/doc/img2.png "ATOS Kernel")

Copyright Alex Boxall 2022. See LICENSE and ATTRIBUTION for details.