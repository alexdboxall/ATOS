#include <syscall.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

int (*syscall_table[SYSCALL_TABLE_SIZE])(size_t args[4]);

int sys_yield(size_t args[4]);

void syscall_init(void) {
    memset(syscall_table, 0, sizeof(syscall_table));
    
    syscall_table[SYSCALL_YIELD] = sys_yield;
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
    
    return syscall_table[call_number](args);
}