#pragma once

#include <stddef.h>

#define MAX_FD_PER_PROCESS 1024

struct filedes_table;
struct vnode;

struct filedes_table* filedes_table_create(void);
struct filedes_table* filedes_table_copy(struct filedes_table* original);
struct vnode* fildesc_convert_to_vnode(struct filedes_table* table, int filedes);
int filedesc_table_register_vnode(struct filedes_table* table, struct vnode* node);
int filedesc_table_deregister_vnode(struct filedes_table* table, struct vnode* node);
int filedes_handle_exec(struct filedes_table* table);