
#include <stddef.h>
#include <thread.h>
#include <errno.h>
#include <vfs.h>
#include <cpu.h>
#include <thread.h>
#include <process.h>
#include <filedes.h>
#include <panic.h>

/*
* ...?
*
* Inputs: 
*         A                 the file descriptor to duplicate
*         B                 (dup2, dup3) the new file descriptor, (dup) not used
*         C                 (dup3) flags, (dup, dup2) not used
*         D                 not used
* Output:
*         >= 0              the duplicated file descriptor
*         < 0               the negative of an error code
*/
int sys_dup(size_t args[4]) {
    return filedesc_table_dup(current_cpu->current_thread->process->fdtable, args[0]);
}

int sys_dup2(size_t args[4]) {
    return filedesc_table_dup2(current_cpu->current_thread->process->fdtable, args[0], args[1]);
}

int sys_dup3(size_t args[4]) {
    return filedesc_table_dup3(current_cpu->current_thread->process->fdtable, args[0], args[1], args[2]);
}