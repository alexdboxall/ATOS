#pragma once

#include <stddef.h>

#define MAX_FD_PER_PROCESS 1024

struct filedes_table;
struct open_file;

struct filedes_table* filedes_table_create(void);
struct filedes_table* filedes_table_copy(struct filedes_table* original);
struct open_file* filedesc_get_open_file(struct filedes_table* table, int filedes);
int filedesc_table_register_file(struct filedes_table* table, struct open_file* node);
int filedesc_table_deregister_file(struct filedes_table* table, struct open_file* node);

int filedesc_table_dup(struct filedes_table* table, int oldfd);
int filedesc_table_dup2(struct filedes_table* table, int oldfd, int newfd);
int filedesc_table_dup3(struct filedes_table* table, int oldfd, int newfd, int flags);

int filedes_handle_exec(struct filedes_table* table);