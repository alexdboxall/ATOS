C:
cd C:/Users/Alex/Desktop/atos

git config --local credential.helper store
git add *
@echo on
python tools/lines.py
set /p COMMIT_MSG="Commit message: "
git commit -m "%COMMIT_MSG%"
@echo on
git push
make osdebug || (pause & exit)

qemu-system-i386 -monitor stdio -soundhw pcspk -hda build/output/disk.bin -serial file:log.txt -m 3 -rtc base=localtime -d guest_errors,cpu_reset 

rem qemu-system-i386 -monitor stdio -soundhw pcspk -fda build/output/floppy.img -serial file:log.txt -m 4 -rtc base=localtime -d guest_errors,cpu_reset 


rem qemu-system-i386 -kernel build/output/kernel -cpu 486 -soundhw pcspk -hda build/output/disk.bin -serial file:log.txt -m 3 -rtc base=localtime -d guest_errors,cpu_reset 
pause