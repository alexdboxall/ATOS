#pragma once

#include <stddef.h>

struct filedes_table;
struct vnode;

struct filedes_table* filedes_table_create(void);
struct filedes_table* filedes_table_copy(struct filedes_table* original);
struct vnode* fildesc_convert_to_vnode(struct filedes_table* table, int filedes, size_t* offset_out);
int filedesc_table_register_vnode(struct filedes_table* table, struct vnode* node);
int filedesc_seek(struct filedes_table* table, int fd, size_t offset);