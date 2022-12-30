
APPLICATION_NAME = usertest.exe

TARGET = x86

CC = i386-banana-gcc
FAKE_CROSS_COMPILER = -m32 -I"." -I"../../../libc/common/include" -I"../../../libc/hosted/include"
COMPILE_FLAGS = -c -Os -fno-strict-aliasing -Wall -Wextra -Wpedantic -Werror -Wcast-align=strict -Wpointer-arith -fmax-errors=5 -std=gnu11 -ffreestanding $(FAKE_CROSS_COMPILER)
LINK_FLAGS = -Wl,-Map=app.map -s -L "../../../libc/$(TARGET)" -nostartfiles -nostdlib -lc -lgcc


COBJECTS = $(patsubst %.c, %.o, $(wildcard *.c) $(wildcard */*.c) $(wildcard */*/*.c) $(wildcard */*/*/*.c) $(wildcard **/*.c))

build: $(COBJECTS)
	$(CC) -T "../../source/machine/application.ld" -o $(APPLICATION_NAME) $^ $(LINK_FLAGS) $(LINKER_STRIP)
	cp $(APPLICATION_NAME) ..\..\output\applications
	
%.o: %.c
	$(CC) $(CPPDEFINES) $(COMPILE_FLAGS) $^ -o $@ 