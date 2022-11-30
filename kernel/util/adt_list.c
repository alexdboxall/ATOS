
/*
* util/adt_stack.c - List Abstract Data Type
*
* A singly-linked list implementation. Elements are a void*, and can be accessed
* either from the front, back, through iteration or by element.
*/

/* TODO: semaphore/mutex for accessing the buckets */

#include <adt.h>
#include <panic.h>
#include <assert.h>
#include <heap.h>

/*
* The internal linked list node.
*/
struct adt_list_node {
    void* data;
    struct adt_list_node* next;
};

struct adt_list {
    /*
    * We implement the list by keeping track of the head and tail for instant insertion
    * times.
    */
    struct adt_list_node* head;
    struct adt_list_node* tail;
    int size;

    /*
    * For iterating over the list
    */
    struct adt_list_node* iter;

    /*
    * We don't support changing the list while it is being iterated over, so
    * enforce that with this flag.
    */
    bool size_changed_since_reset;
};


/*
* Allocate and initialise a list with no elements.
*/
struct adt_list* adt_list_create(void) {
    struct adt_list* list = malloc(sizeof(struct adt_list));

    list->head = NULL;
    list->tail = NULL;
    list->iter = NULL;
    list->size = 0;
    list->size_changed_since_reset = false;

    return list;
}

void adt_list_destroy(struct adt_list* list) {
    /*
    * Clear (and hence free) all nodes in the list.
    */
    while (adt_list_size(list) > 0) {
        adt_list_remove_front(list);
    }

    /*
    * Now we can free the list itself.
    */
    free(list);
}


/*
* Allocate a new internal node with the specified data.
*/
static struct adt_list_node* adt_list_allocate_node(void* data) {
    struct adt_list_node* node = malloc(sizeof(struct adt_list_node));
    node->data = data;
    node->next = NULL;
    return node;    
}

void adt_list_add_front(struct adt_list* list, void* data) {
    struct adt_list_node* new_node = adt_list_allocate_node(data);

    if (list->size == 0) {
        assert(list->tail == NULL);
        assert(list->head == NULL);

        list->tail = new_node;

    } else {
        assert(list->tail != NULL);
        assert(list->head != NULL);
        
        new_node->next = list->head;
    }

    list->head = new_node;
    list->size_changed_since_reset = true;
    list->size++;  
}

void adt_list_add_back(struct adt_list* list, void* data) {
    struct adt_list_node* new_node = adt_list_allocate_node(data);

    if (list->size == 0) {
        assert(list->tail == NULL);
        assert(list->head == NULL);

        list->head = new_node;

    } else {
        assert(list->tail != NULL);
        assert(list->head != NULL);
        
        list->tail->next = new_node;
    }

    list->tail = new_node;
    list->size_changed_since_reset = true;
    list->size++;  
}

void* adt_list_remove_front(struct adt_list* list) {
    if (list->size == 0) {
        panic("adt_list_remove_front: list is empty");
    }

    assert(list->head != NULL);
    assert(list->tail != NULL);

    /*
    * Remove the head node.
    */
    struct adt_list_node* old_head = list->head;
    list->head = list->head->next;

    /*
    * Check for a newly-empty list.
    */
    if (list->head == NULL) {
        assert(list->tail != NULL);
        list->tail = NULL;
    }

    list->size--;
    list->size_changed_since_reset = true;

    /*
    * Ensure we free the node we deleted.
    */
    void* data = old_head->data;
    free(old_head);

    return data;
}

void* adt_list_remove_back(struct adt_list* list) {
    if (list->size == 0) {
        panic("adt_list_remove_back: list is empty");
    }

    assert(list->head != NULL);
    assert(list->tail != NULL);

    /*
    * Remove the tail node.
    */
    struct adt_list_node* old_tail = list->tail;
    if (list->head == list->tail) {
        list->head = NULL;
        list->tail = NULL;

    } else {
        /*
        * We can trash list->iter as the size of the list will change, and 
        * therefore adt_list_reset() must be called before iterating (otherwise we will panic).
        * 
        * This cannot create an empty list, as we checked for lists with only one
        * element before we got here.
        * 
        * (We could probably use the existing value of list->iter if it isn't already at the tail,
        *  but let's not do that for now.)
        */
        list->iter = list->head;
        assert(list->iter != NULL);

        while (list->iter) {
            if (list->iter->next == list->tail) {
                list->iter->next = NULL;
                list->tail = list->iter;
                break;
            }

            list->iter = list->iter->next;
        }
    }

    list->size--;
    list->size_changed_since_reset = true;

    /*
    * Ensure we free the node we deleted.
    */
    void* data = old_tail->data;
    free(old_tail);

    return data;
}

void* adt_list_get_front(struct adt_list* list) {
    if (list->size == 0) {
        panic("adt_list_get_front: list is empty");
    }

    return list->head->data;
}

void* adt_list_get_back(struct adt_list* list)  {
    if (list->size == 0) {
        panic("adt_list_get_back: list is empty");
    }

    return list->tail->data;
}

int adt_list_size(struct adt_list* list) {
    return list->size;
}

/*
* Reset the iterator to the start of the list.
*/
void adt_list_reset(struct adt_list* list) {
    list->size_changed_since_reset = false;
    list->iter = list->head;
}

/*
* Returns true if the iterator is not yet at the end of the list (i.e. you can
* call adt_list_get_next()).
*/
bool adt_list_has_next(struct adt_list* list) {
    if (list->size_changed_since_reset) {
        panic("adt_list_has_next: size changed since last reset");
    }

    return list->iter != NULL;
}

/*
* Return the next element in the list. Before calling this, it should be checked
* that the iterator has not reached the end of the list.
*/
void* adt_list_get_next(struct adt_list* list) {
    if (list->size_changed_since_reset) {
        panic("adt_list_get_next: size changed since last reset");
    }

    if (list->iter == NULL) {
        panic("adt_list_get_next: no next element");
    }

    void* data = list->iter->data;   
    list->iter = list->iter->next;

    return data;
}


/*
* Returns the data at the front of the list, and then moves it 
* to the back of the list.
*/
void* adt_list_circulate(struct adt_list* list) {
    /*
    * This could be implemented more efficiently so that nodes aren't 
    * destroyed and then immediately recreated, but it's not worth it.
    *
    * (If you do implement it, you should probably make sure that you
    *  set list->size_changed_since_reset to true, as although the size
    *  hasn't changed, the order has)
    */

    void* data = adt_list_remove_front(list);
    adt_list_add_back(list, data);
    return data;
}


/*
* Removes the first instance of an element with the given data. Returns
* whether or not the element was found.
*/
bool adt_list_remove_element(struct adt_list* list, void* data) {
    if (list->size == 0) {
        panic("adt_list_remove_element: list is empty");
    }

    assert(list->head != NULL);
    assert(list->tail != NULL);

    /*
    * We can trash list->iter as the size of the list will change.
    */

    list->iter = list->head;

    struct adt_list_node* prev = NULL;
    while (list->iter) {
        if (list->iter->data == data) {
            if (prev == NULL) {
                /* Removing the first element. */
                list->head = list->iter->next;

                /* Check for a newly-empty list. */
                if (list->head == NULL) {
                    list->tail = NULL;
                }

            } else {
                /* Removing any other element */
                prev->next = list->iter->next;
            }

            /* Set the tail if we're removing the last element. */
            if (list->iter == list->tail) {
                list->tail = prev;

                /* Check for a newly-empty list. */
                if (list->tail == NULL) {
                    list->head = NULL;
                }
            }

            list->size_changed_since_reset = true;
            list->size--;

            return true;
        }

        prev = list->iter;
        list->iter = list->iter->next;
    }

    return false;    
}
