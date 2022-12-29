#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <syscall.h>
#include <syscallnum.h>
#include <kprintf.h>

int (*syscall_table[SYSCALL_TABLE_SIZE])(size_t args[4]);

int sys_yield(size_t args[4]);
int sys_terminate(size_t args[4]);
int sys_open(size_t args[4]);
int sys_read(size_t args[4]);
int sys_write(size_t args[4]);
int sys_close(size_t args[4]);
int sys_lseek(size_t args[4]);

void syscall_init(void) {
    memset(syscall_table, 0, sizeof(syscall_table));
    
    syscall_table[SYSCALL_YIELD] = sys_yield;
    syscall_table[SYSCALL_TERMINATE] = sys_terminate;
    syscall_table[SYSCALL_OPEN] = sys_open;
    syscall_table[SYSCALL_READ] = sys_read;
    syscall_table[SYSCALL_WRITE] = sys_write;
    syscall_table[SYSCALL_CLOSE] = sys_close;
    syscall_table[SYSCALL_LSEEK] = sys_lseek;
}

/*
* This needs to be called from an arch_ interrupt handler.
*/
int syscall_perform(int call_number, size_t args[4]) {
    if (call_number < 0 || call_number >= SYSCALL_TABLE_SIZE) {
        return ENOSYS;
    }

    if (syscall_table[call_number] == NULL) {
        return ENOSYS;
    }

    kprintf("SYSCALL %d: 0x%X, 0x%X, 0x%X 0x%X\n", call_number,
        args[0], args[1], args[2], args[3]);
    
    return syscall_table[call_number](args);
}