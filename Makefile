
#override this by using TARGET=... when calling make
TARGET=x86

ROOT_BUILD_DIR = ./build
BUILD_DRIVER_SOURCE_DIR = $(ROOT_BUILD_DIR)/drvsrc
BUILD_APP_SOURCE_DIR = $(ROOT_BUILD_DIR)/appsrc
BUILD_SOURCE_DIR = $(ROOT_BUILD_DIR)/source
BUILD_OUTPUT_DIR = $(ROOT_BUILD_DIR)/output
KERNEL_DIR = ./kernel
DRIVER_DIR = ./drivers/$(TARGET)
COMMON_DRIVER_DIR = ./drivers/common
APP_DIR = ./applications
FREESTANDING_LIBC_DIR = ./libc/common
LIBC_MAKEFILE_DIR = ./libc/$(TARGET)
SYSROOT = ./sysroot

SUBDIRS := 

debug_compile:
	$(MAKE) -C $(BUILD_SOURCE_DIR)
	
release_compile:
	rm -r $(BUILD_SOURCE_DIR)/test/internal
	$(MAKE) -C $(BUILD_SOURCE_DIR) CPPDEFINES=-DNDEBUG 
	# we can't set LINKER_STRIP=-s, as it removes .symtab and .strtab, which we need
	
common_header:
	/usr/bin/find $(KERNEL_DIR) -type f -name '*.o' -delete
	/usr/bin/find ./libc -type f -name '*.o' -delete
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
	rm $(BUILD_OUTPUT_DIR)/drivers/TEMPLATE.SYS
	rm $(BUILD_OUTPUT_DIR)/applications/TEMPLATE.EXE
	cp -r $(BUILD_OUTPUT_DIR)/drivers/* $(SYSROOT)/System
	cp -r $(BUILD_OUTPUT_DIR)/applications/* $(SYSROOT)/System
	cp $(BUILD_OUTPUT_DIR)/kernel.exe $(SYSROOT)/System
	gcc ./tools/mkdemofs.cpp -o ./tools/mkdemofs
	chmod 777 ./tools/mkdemofs
	./tools/mkdemofs 64 $(BUILD_OUTPUT_DIR)/kernel.exe $(SYSROOT) $(BUILD_OUTPUT_DIR)/disk.bin bootloader.bin
	head -c 1474560 $(BUILD_OUTPUT_DIR)/disk.bin >> $(BUILD_OUTPUT_DIR)/floppy.img


app_header:
	mkdir $(BUILD_APP_SOURCE_DIR)
	mkdir $(BUILD_OUTPUT_DIR)/applications
	cp -r $(APP_DIR)/* $(BUILD_APP_SOURCE_DIR)

app_all:
	for dir in $(wildcard ./$(BUILD_APP_SOURCE_DIR)/*/.); do \
        $(MAKE) -C $$dir build; \
    done
	
	
driver_header:
	mkdir $(BUILD_DRIVER_SOURCE_DIR)
	mkdir $(BUILD_DRIVER_SOURCE_DIR)/drivers
	mkdir $(BUILD_OUTPUT_DIR)/drivers
	cp -r $(DRIVER_DIR)/* $(BUILD_DRIVER_SOURCE_DIR)/drivers
	cp -r $(COMMON_DRIVER_DIR)/* $(BUILD_DRIVER_SOURCE_DIR)/drivers

driver_all:
	for dir in $(wildcard ./$(BUILD_DRIVER_SOURCE_DIR)/drivers/*/.); do \
        $(MAKE) -C $$dir build; \
    done
	
cstdlib:
	$(MAKE) -C $(LIBC_MAKEFILE_DIR)

applications: app_header app_all
drivers: driver_header driver_all
osrelease: common_header release_compile cstdlib drivers applications common_footer
osdebug: common_header debug_compile cstdlib drivers applications common_footer
