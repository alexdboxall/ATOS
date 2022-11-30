
#include <adt.h>
#include <panic.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <kprintf.h>
#include <string.h>
#include <heap.h>

/* TODO: semaphore/mutex for accessing the buckets */

struct adt_hashmap {
    uint32_t (*hash_function)(void*);
    bool (*equality_function)(void*, void*);

    struct adt_list** buckets;

    int num_buckets;
    int size;
};

struct node {
    void* key;
    void* value;
};

struct adt_hashmap* adt_hashmap_create(int num_buckets, uint32_t (*hash_function)(void*), bool (*equality_function)(void*, void*)) {
    struct adt_hashmap* map = malloc(sizeof(struct adt_hashmap));

    if (num_buckets < 1) {
        panic("adt_hashmap_create: cannot have less than 1 bucket!");
    }

    map->equality_function = equality_function;
    map->hash_function = hash_function;
    map->num_buckets = num_buckets;
    map->size = 0;
    map->buckets = malloc(sizeof(struct adt_list*) * num_buckets);

    for (int i = 0; i < num_buckets; ++i) {
        map->buckets[i] = NULL;
    }

    return map;
}

bool adt_hashmap_contains(struct adt_hashmap* map, void* key) {
    uint32_t bucket = map->hash_function(key) % map->num_buckets;

    if (map->buckets[bucket] == NULL) {
        return NULL;
    }

    struct adt_list* list = map->buckets[bucket];

    adt_list_reset(list);
    while (adt_list_has_next(list)) {
        struct node* item = adt_list_get_next(list);

        if (item != NULL) {
            if (map->equality_function(item->key, key)) {
                return true;
            }
        }
    }

    return false;
}

void adt_hashmap_add(struct adt_hashmap* map, void* key, void* value) {
    uint32_t bucket = map->hash_function(key) % map->num_buckets;

    if (map->buckets[bucket] == NULL) {
        map->buckets[bucket] = adt_list_create();
    }

    struct adt_list* list = map->buckets[bucket];

    struct node* node = malloc(sizeof(struct node));
    node->key = key;
    node->value = value;

    map->size++;

    adt_list_add_back(list, node);
}

void* adt_hashmap_get(struct adt_hashmap* map, void* key) {
    uint32_t bucket = map->hash_function(key) % map->num_buckets;

    if (map->buckets[bucket] == NULL) {
        return NULL;
    }

    struct adt_list* list = map->buckets[bucket];

    adt_list_reset(list);
    while (adt_list_has_next(list)) {
        struct node* item = adt_list_get_next(list);

        if (item != NULL) {
            if (map->equality_function(item->key, key)) {
                return item->value;
            }
        }
    }

    panic("adt_hashmap_get: not in map. please call adt_hashmap_contains first!");
    return NULL;
}

int adt_hashmap_size(struct adt_hashmap* map) {
    return map->size;
}

uint32_t adt_hashmap_null_terminated_string_hash_function(void* arg) {
    char* string = (char*) arg;

    /* 
     * A simple hash algorithm from here: 
     * http://www.cse.yorku.ca/~oz/hash.html
    */

    uint32_t hash = 5381;
    for (int i = 0; string[i]; ++i) {
        hash = hash * 33 + string[i];
    }

    return hash;
}

bool adt_hashmap_null_terminated_string_equality_function(void* s1, void* s2) {
    return !strcmp((const char*) s1, (const char*) s2);
}