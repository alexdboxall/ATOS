qemu-system-i386 -kernel build/output/kernel -soundhw pcspk -serial file:debuglog.txt -m 32 -rtc base=localtime -d guest_errors,cpu_reset 
pause