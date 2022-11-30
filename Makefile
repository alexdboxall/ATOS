
#override this by using TARGET=... when calling make
TARGET=x86

ROOT_BUILD_DIR = ./build
BUILD_DRIVER_SOURCE_DIR = $(ROOT_BUILD_DIR)/drvsrc
BUILD_SOURCE_DIR = $(ROOT_BUILD_DIR)/source
BUILD_OUTPUT_DIR = $(ROOT_BUILD_DIR)/output
KERNEL_DIR = ./kernel
DRIVER_DIR = ./drivers
FREESTANDING_LIBC_DIR = ./libc/common
SYSROOT = ./sysroot

SUBDIRS := 

debug_compile:
	$(MAKE) -C $(BUILD_SOURCE_DIR)
	
release_compile:
	rm -r $(BUILD_SOURCE_DIR)/test/internal
	$(MAKE) -C $(BUILD_SOURCE_DIR) CPPDEFINES=-DNDEBUG LINKER_STRIP=-s
	
common_header:
	rm -r $(ROOT_BUILD_DIR) || true
	rm -r $(SYSROOT)/System || true
	mkdir $(SYSROOT)/System
	mkdir $(ROOT_BUILD_DIR)
	mkdir $(BUILD_SOURCE_DIR)
	mkdir $(BUILD_OUTPUT_DIR)
	cp -r $(KERNEL_DIR)/* $(BUILD_SOURCE_DIR)
	rm -r $(BUILD_SOURCE_DIR)/arch
	mkdir $(BUILD_SOURCE_DIR)/machine
	cp -r $(KERNEL_DIR)/arch/$(TARGET)/* $(BUILD_SOURCE_DIR)/machine
	rm -r $(BUILD_SOURCE_DIR)/machine/include
	cp -r $(KERNEL_DIR)/arch/$(TARGET)/include/* $(BUILD_SOURCE_DIR)/machine
	cp -r $(FREESTANDING_LIBC_DIR)/* $(BUILD_SOURCE_DIR)/
	

common_footer:
	cp $(BUILD_SOURCE_DIR)/kernel.exe $(BUILD_OUTPUT_DIR)/kernel.exe
	cp $(BUILD_SOURCE_DIR)/kernel.map $(BUILD_OUTPUT_DIR)/kernel.map
	objdump -drwC -Mintel $(BUILD_OUTPUT_DIR)/kernel.exe >> $(BUILD_OUTPUT_DIR)/disassembly.txt
	nasm bootloader.s -o bootloader.bin
	cp -r $(BUILD_OUTPUT_DIR)/drivers/* $(SYSROOT)/System
	cp $(BUILD_OUTPUT_DIR)/kernel.exe $(SYSROOT)/System
	./tools/mkdemofs.exe 64 $(BUILD_OUTPUT_DIR)/kernel.exe $(SYSROOT) $(BUILD_OUTPUT_DIR)/disk.bin bootloader.bin
	head -c 1474560 $(BUILD_OUTPUT_DIR)/disk.bin >> $(BUILD_OUTPUT_DIR)/floppy.img


driver_header:
	mkdir $(BUILD_DRIVER_SOURCE_DIR)
	mkdir $(BUILD_OUTPUT_DIR)/drivers
	cp -r $(DRIVER_DIR) $(BUILD_DRIVER_SOURCE_DIR)

driver_all:
	for dir in $(wildcard ./$(BUILD_DRIVER_SOURCE_DIR)/drivers/*/.); do \
        $(MAKE) -C $$dir build; \
    done
	
	
drivers: driver_header driver_all
osrelease: common_header release_compile drivers common_footer
osdebug: common_header debug_compile drivers common_footer
