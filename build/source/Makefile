
CC = i386-banana-gcc
AS = nasm
FAKE_CROSS_COMPILER = -m32 -I"." -Iinclude -Imachine/include
COMPILE_FLAGS = -c -Os -fno-strict-aliasing -DCOMPILE_KERNEL -Wall -Wextra -Wpedantic -Werror -Wcast-align=strict -Wpointer-arith -fmax-errors=5 -std=gnu11 -ffreestanding $(FAKE_CROSS_COMPILER)
LINK_FLAGS = -Wl,-Map=kernel.map -nostartfiles -nostdlib -lgcc 

# Set by the higher level Makefile before calling us - changes depending on whether we are compiling the debug or release build
LINKER_STRIP =
CPPDEFINES = 

COBJECTS = $(patsubst %.c, %.o, $(wildcard *.c) $(wildcard */*.c) $(wildcard */*/*.c) $(wildcard */*/*/*.c) $(wildcard **/*.c))
ASMOBJECTS = $(patsubst %.s, %.oo, $(wildcard *.s) $(wildcard */*.s) $(wildcard */*/*.s) $(wildcard */*/*/*.s) $(wildcard **/*.s))

oskernel: $(COBJECTS) $(ASMOBJECTS)
	$(CC) -T machine/linker.ld -o KERNEL.EXE $^ $(LINK_FLAGS) $(LINKER_STRIP)
	
%.o: %.c
	$(CC) $(CPPDEFINES) $(COMPILE_FLAGS) $^ -o $@ 

%.oo: %.s
	$(AS) -felf32 $^ -o $@ 
