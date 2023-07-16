
make osrelease || (pause & exit)
qemu-system-i386 -monitor stdio -hda build/output/disk.bin -serial file:log.txt -m 3 -rtc base=localtime -d guest_errors,cpu_reset 
rem qemu-system-i386 -monitor stdio -fda build/output/floppy.img -serial file:log.txt -m 3 -rtc base=localtime -d guest_errors,cpu_reset 

read -p "Press Enter to continue" </dev/tty
