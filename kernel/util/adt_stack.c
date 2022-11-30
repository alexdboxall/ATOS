
/*
* util/adt_stack.c - Stack Abstract Data Type
*
*/

#include <adt.h>
#include <panic.h>
#include <assert.h>
#include <heap.h>

/*
* What is a stack? A miserable little wrapper around a list.
*/

struct adt_stack {
    struct adt_list* list;
};

struct adt_stack* adt_stack_create(void) {
    struct adt_stack* stack = malloc(sizeof(struct adt_stack));
    stack->list = adt_list_create();
    return stack;
}

void adt_stack_destroy(struct adt_stack* stack) {
    adt_list_destroy(stack->list);
    free(stack);
}

void adt_stack_push(struct adt_stack* stack, void* data) {
    adt_list_add_front(stack->list, data);
}

void* adt_stack_pop(struct adt_stack* stack) {
    return adt_list_remove_front(stack->list);
}

int adt_stack_size(struct adt_stack* stack) {
    return adt_list_size(stack->list);
}