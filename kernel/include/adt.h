#pragma once

#include <common.h>

/*
* TODO: OS-wide: _init should initialise an existing object (or be a bootstrap call with no args)
*                create/alloc should allocate and then initialise all in one
*                (should you have seperate alloc/init? maybe not, it might get confusing)
*                destroy should be for things that were allocated (i.e. have create/destroy)
*                cleanup should be for things that were initialised (i.e. have init/cleanup)
*/


/*
* We don't want to reveal any of the internals to other files, definitions are
* alongside the implementions in util/adt...
*/
struct adt_stack;
struct adt_list;
struct adt_blocking_byte_buffer;
struct adt_hashmap;

struct adt_blocking_byte_buffer* adt_blocking_byte_buffer_create(int size);
void adt_blocking_byte_buffer_destroy(struct adt_blocking_byte_buffer* buffer);
void adt_blocking_byte_buffer_add(struct adt_blocking_byte_buffer* buffer, uint8_t c);
uint8_t adt_blocking_byte_buffer_get(struct adt_blocking_byte_buffer* buffer);
int adt_blocking_byte_buffer_try_get(struct adt_blocking_byte_buffer* buffer, uint8_t* c);

struct adt_stack* adt_stack_create(void);
void adt_stack_destroy(struct adt_stack* stack);
void adt_stack_push(struct adt_stack* stack, void* data);
void* adt_stack_pop(struct adt_stack* stack);
int adt_stack_size(struct adt_stack* stack);

struct adt_list* adt_list_create(void);
void adt_list_destroy(struct adt_list* list);
void adt_list_add_front(struct adt_list* list, void* data);
void adt_list_add_back(struct adt_list* list, void* data);
void* adt_list_get_front(struct adt_list* list);
void* adt_list_get_back(struct adt_list* list);
void* adt_list_remove_front(struct adt_list* list);
void* adt_list_remove_back(struct adt_list* list);
int adt_list_size(struct adt_list* list);

/*
* Removes the first instance of a data item from the list.
*/
bool adt_list_remove_element(struct adt_list* list, void* data);

/*
* If the size of the list is changed, reset must be called before 
* has_next or get_next are used.
*/
void adt_list_reset(struct adt_list* list);
bool adt_list_has_next(struct adt_list* list);
void* adt_list_get_next(struct adt_list* list);

/*
* Returns the first item, and then moves it to the back. Useful
* for getting circular linked-list-style functionality.
*/
void* adt_list_circulate(struct adt_list* list);




struct adt_hashmap* adt_hashmap_create(int num_buckets, uint32_t (*hash_function)(void*), bool (*equality_function)(void*, void*)); 
bool adt_hashmap_contains(struct adt_hashmap* map, void* key);
void adt_hashmap_add(struct adt_hashmap* map, void* key, void* value);
void* adt_hashmap_get(struct adt_hashmap* map, void* key);
int adt_hashmap_size(struct adt_hashmap* map);

uint32_t adt_hashmap_null_terminated_string_hash_function(void* arg);
bool adt_hashmap_null_terminated_string_equality_function(void* s1, void* s2);