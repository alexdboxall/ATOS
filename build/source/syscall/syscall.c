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
int sys_sbrk(size_t args[4]);
int sys_isatty(size_t args[4]);
int sys_dup(size_t args[4]);
int sys_dup2(size_t args[4]);
int sys_dup3(size_t args[4]);
int sys_tcgetattr(size_t args[4]);
int sys_tcsetattr(size_t args[4]);

void syscall_init(void) {
    memset(syscall_table, 0, sizeof(syscall_table));
    
    syscall_table[SYSCALL_YIELD] = sys_yield;
    syscall_table[SYSCALL_TERMINATE] = sys_terminate;
    syscall_table[SYSCALL_OPEN] = sys_open;
    syscall_table[SYSCALL_READ] = sys_read;
    syscall_table[SYSCALL_WRITE] = sys_write;
    syscall_table[SYSCALL_CLOSE] = sys_close;
    syscall_table[SYSCALL_LSEEK] = sys_lseek;
    syscall_table[SYSCALL_SBRK] = sys_sbrk;
    syscall_table[SYSCALL_ISATTY] = sys_isatty;
    syscall_table[SYSCALL_DUP] = sys_dup;
    syscall_table[SYSCALL_DUP2] = sys_dup2;
    syscall_table[SYSCALL_DUP3] = sys_dup3;
    syscall_table[SYSCALL_TCGETATTR] = sys_tcgetattr;
    syscall_table[SYSCALL_TCSETATTR] = sys_tcsetattr;
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

    //kprintf("SYSCALL %d: 0x%X, 0x%X, 0x%X 0x%X\n", call_number, args[0], args[1], args[2], args[3]);
    
    return syscall_table[call_number](args);
}