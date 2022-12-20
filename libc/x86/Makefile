
CC = i386-banana-gcc
AR = i386-banana-ar
AS = nasm
FAKE_CROSS_COMPILER = -m32 -I../common/include -I../hosted/include
COMPILE_FLAGS = -c -Os -fno-strict-aliasing -Wall -Wextra -Wpedantic -Werror -Wcast-align=strict -Wpointer-arith -fmax-errors=5 -std=gnu11 -ffreestanding $(FAKE_CROSS_COMPILER)
LINK_FLAGS = -nostartfiles -nostdlib -lgcc 

# Set by the higher level Makefile before calling us - changes depending on whether we are compiling the debug or release build
LINKER_STRIP =
CPPDEFINES = 

COBJECTS = $(patsubst %.c, %.o, $(wildcard ../*.c) $(wildcard ../*/*.c) $(wildcard ../*/*/*.c) $(wildcard ../*/*/*/*.c) $(wildcard ../**/*.c))
ASMOBJECTS = $(patsubst %.s, %.oo, $(wildcard *.s) $(wildcard */*.s) $(wildcard */*/*.s) $(wildcard */*/*/*.s) $(wildcard **/*.s))

hostedlibc: $(COBJECTS) $(ASMOBJECTS)
	$(AS) -felf32 _systemcall.s -o _systemcall.o
	$(AR) rcs libc.a $^ _systemcall.o
	rm ../common/*.o
	rm ../hosted/*.o
	rm _systemcall.o
	
%.o: %.c
	$(CC) $(CPPDEFINES) $(COMPILE_FLAGS) $^ -o $@ 
	
%.oo: %.s
	$(AS) -felf32 $^ -o $@ 
