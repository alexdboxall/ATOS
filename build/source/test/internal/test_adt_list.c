
#include <assert.h>
#include <string.h>
#include <adt.h>
#include <test.h>

void test_basic_insert_back_and_iter(void) {
    BEGIN_TEST("basic insert_back and iter");

    struct adt_list* list = adt_list_create();

    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");
    adt_list_add_back(list, "4");

    assert(adt_list_size(list) == 4);

    adt_list_reset(list);

    assert(adt_list_has_next(list));
    char* a = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* b = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* c = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* d = adt_list_get_next(list);
    assert(!adt_list_has_next(list));

    assert(!strcmp(a, "1"));
    assert(!strcmp(b, "2"));
    assert(!strcmp(c, "3"));
    assert(!strcmp(d, "4"));

    adt_list_reset(list);
    assert(adt_list_has_next(list));
    char* e = adt_list_get_next(list);
    assert(!strcmp(e, "1"));

    adt_list_destroy(list);
    END_TEST();
}

void test_basic_insert_front_and_iter(void) {
    BEGIN_TEST("basic insert_front and iter");
    struct adt_list* list = adt_list_create();

    adt_list_add_front(list, "1");
    adt_list_add_front(list, "2");
    adt_list_add_front(list, "3");
    adt_list_add_front(list, "4");

    assert(adt_list_size(list) == 4);

    adt_list_reset(list);

    assert(adt_list_has_next(list));
    char* a = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* b = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* c = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* d = adt_list_get_next(list);
    assert(!adt_list_has_next(list));

    assert(!strcmp(a, "4"));
    assert(!strcmp(b, "3"));
    assert(!strcmp(c, "2"));
    assert(!strcmp(d, "1"));

    adt_list_reset(list);
    assert(adt_list_has_next(list));
    char* e = adt_list_get_next(list);
    assert(!strcmp(e, "4"));

    adt_list_destroy(list);
    END_TEST();
}

void test_circulate(void) {
    BEGIN_TEST("circulate");
    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");
    
    assert(!strcmp(adt_list_circulate(list), "1"));
    assert(!strcmp(adt_list_circulate(list), "2"));
    assert(!strcmp(adt_list_circulate(list), "3"));
    assert(!strcmp(adt_list_circulate(list), "1"));
    assert(!strcmp(adt_list_circulate(list), "2"));
    assert(!strcmp(adt_list_circulate(list), "3"));
    assert(!strcmp(adt_list_circulate(list), "1"));

    adt_list_destroy(list);
    END_TEST();
}

void test_basic_get_head_and_tail(void) {
    BEGIN_TEST("basic get_head and tail");
    struct adt_list* list = adt_list_create();

    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");
    
    assert(!strcmp(adt_list_get_front(list), "1"));
    assert(!strcmp(adt_list_get_back(list), "3"));
    assert(!strcmp(adt_list_get_front(list), "1"));
    assert(!strcmp(adt_list_get_back(list), "3"));

    adt_list_destroy(list);
    END_TEST();
}

void test_insert_alternating(void) {
    BEGIN_TEST("insert alternating");

    struct adt_list* list = adt_list_create();
    adt_list_add_front(list, "2");
    adt_list_add_back(list, "3");
    adt_list_add_front(list, "1");
    adt_list_add_back(list, "4");
    adt_list_add_back(list, "5");

    adt_list_reset(list);
    char* a = adt_list_get_next(list);
    char* b = adt_list_get_next(list);
    char* c = adt_list_get_next(list);
    char* d = adt_list_get_next(list);
    char* e = adt_list_get_next(list);

    assert(!strcmp(a, "1"));
    assert(!strcmp(b, "2"));
    assert(!strcmp(c, "3"));
    assert(!strcmp(d, "4"));
    assert(!strcmp(e, "5"));

    adt_list_destroy(list);
    END_TEST();
}

void test_insert_back_updates_tail_correctly(void) {
    BEGIN_TEST("insert back updates tail correctly");
    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");

    assert(!strcmp(adt_list_get_back(list), "3"));
    adt_list_add_back(list, "4");
    assert(!strcmp(adt_list_get_back(list), "4"));
    adt_list_add_back(list, "5");
    assert(!strcmp(adt_list_get_back(list), "5"));

    adt_list_destroy(list);
    END_TEST();
}

void test_insert_front_updates_head_correctly(void) {
    BEGIN_TEST("insert front updates head correctly");
    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "3");
    adt_list_add_back(list, "4");
    adt_list_add_back(list, "5");
    
    assert(!strcmp(adt_list_get_front(list), "3"));
    adt_list_add_front(list, "2");
    assert(!strcmp(adt_list_get_front(list), "2"));
    adt_list_add_front(list, "1");
    assert(!strcmp(adt_list_get_front(list), "1"));

    adt_list_destroy(list);
    END_TEST();
}

void test_remove_back_updates_tail_correctly() {
    BEGIN_TEST("remove back updates tail correctly");
    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");

    assert(adt_list_size(list) == 3);
    assert(!strcmp(adt_list_get_back(list), "3"));

    adt_list_remove_back(list);
    assert(!strcmp(adt_list_get_back(list), "2"));
    assert(adt_list_size(list) == 2);

    adt_list_remove_back(list);
    assert(!strcmp(adt_list_get_back(list), "1"));
    assert(adt_list_size(list) == 1);

    adt_list_remove_back(list);
    assert(adt_list_size(list) == 0);

    adt_list_destroy(list);
    END_TEST();
}

void test_remove_front_updates_head_correctly() {
    BEGIN_TEST("remove front updates head correctly");
    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");
    
    assert(adt_list_size(list) == 3);
    assert(!strcmp(adt_list_get_front(list), "1"));
    
    adt_list_remove_front(list);
    assert(!strcmp(adt_list_get_front(list), "2"));
    assert(adt_list_size(list) == 2);
    
    adt_list_remove_front(list);
    assert(!strcmp(adt_list_get_front(list), "3"));
    assert(adt_list_size(list) == 1);
    
    adt_list_remove_front(list);
    assert(adt_list_size(list) == 0);

    adt_list_destroy(list);
    END_TEST();
}

void test_remove_both_ends() {
    BEGIN_TEST("remove both ends");
    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");
    
    assert(adt_list_size(list) == 3);
    assert(!strcmp(adt_list_get_front(list), "1"));
    assert(!strcmp(adt_list_get_back(list), "3"));
    
    adt_list_remove_front(list);
    assert(!strcmp(adt_list_get_front(list), "2"));
    assert(!strcmp(adt_list_get_back(list), "3"));
    assert(adt_list_size(list) == 2);

    adt_list_remove_back(list);
    assert(!strcmp(adt_list_get_front(list), "2"));
    assert(!strcmp(adt_list_get_back(list), "2"));
    assert(adt_list_size(list) == 1);

    adt_list_remove_back(list);
    assert(adt_list_size(list) == 0);

    adt_list_destroy(list);
    END_TEST();
}

void test_remove_final_via_back(void) {
    BEGIN_TEST("remove final via back");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");

    assert(adt_list_size(list) == 2);
    assert(!strcmp(adt_list_get_back(list), "2"));
    assert(!strcmp(adt_list_get_front(list), "1"));

    adt_list_remove_back(list);
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_back(list), "1"));
    assert(!strcmp(adt_list_get_front(list), "1"));

    adt_list_remove_back(list);
    assert(adt_list_size(list) == 0);

    adt_list_add_back(list, "3");
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_back(list), "3"));
    assert(!strcmp(adt_list_get_front(list), "3"));

    adt_list_remove_front(list);
    assert(adt_list_size(list) == 0);

    adt_list_add_back(list, "3");
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_back(list), "3"));
    assert(!strcmp(adt_list_get_front(list), "3"));

    adt_list_remove_back(list);
    assert(adt_list_size(list) == 0);

    adt_list_destroy(list);
    END_TEST();
}

void test_remove_final_via_front(void) {
    BEGIN_TEST("remove final via front");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");

    assert(adt_list_size(list) == 2);
    assert(!strcmp(adt_list_get_back(list), "2"));
    assert(!strcmp(adt_list_get_front(list), "1"));

    adt_list_remove_front(list);
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_back(list), "2"));
    assert(!strcmp(adt_list_get_front(list), "2"));

    adt_list_remove_front(list);
    assert(adt_list_size(list) == 0);
    
    adt_list_add_back(list, "3");
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_back(list), "3"));
    assert(!strcmp(adt_list_get_front(list), "3"));

    adt_list_remove_front(list);
    assert(adt_list_size(list) == 0);

    adt_list_add_back(list, "3");
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_back(list), "3"));
    assert(!strcmp(adt_list_get_front(list), "3"));

    adt_list_remove_back(list);
    assert(adt_list_size(list) == 0);

    adt_list_add_back(list, "3");
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_back(list), "3"));
    assert(!strcmp(adt_list_get_front(list), "3"));

    adt_list_remove_front(list);
    assert(adt_list_size(list) == 0);

    adt_list_destroy(list);
    END_TEST();
}

void test_add_first_via_front(void) {
    BEGIN_TEST("add first via front");

    struct adt_list* list = adt_list_create();
    adt_list_add_front(list, "1");
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_front(list), "1"));
    assert(!strcmp(adt_list_get_back(list), "1"));

    adt_list_destroy(list);
    END_TEST();
}

void test_add_first_via_back(void) {
    BEGIN_TEST("add first via back");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    assert(adt_list_size(list) == 1);
    assert(!strcmp(adt_list_get_front(list), "1"));
    assert(!strcmp(adt_list_get_back(list), "1"));

    adt_list_destroy(list);
    END_TEST();
}

void test_list_remove_element_first(void) {
    BEGIN_TEST("list remove element first");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");
    adt_list_add_back(list, "4");
    adt_list_add_back(list, "5");

    assert(adt_list_size(list) == 5);

    adt_list_remove_element(list, "1");
    assert(adt_list_size(list) == 4);

    adt_list_remove_element(list, "2");
    assert(adt_list_size(list) == 3);

    adt_list_reset(list);
    assert(adt_list_has_next(list));
    char* a = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* b = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* c = adt_list_get_next(list);
    assert(!adt_list_has_next(list));
    assert(adt_list_size(list) == 3);

    assert(!strcmp(a, "3"));
    assert(!strcmp(b, "4"));
    assert(!strcmp(c, "5"));

    adt_list_destroy(list);
    END_TEST();
}

void test_list_remove_element_last(void) {
    BEGIN_TEST("list remove element last");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "a");
    adt_list_add_back(list, "b");
    adt_list_add_back(list, "c");
    adt_list_add_back(list, "d");
    adt_list_add_back(list, "e");

    assert(adt_list_size(list) == 5);

    adt_list_remove_element(list, "e");
    assert(adt_list_size(list) == 4);

    adt_list_remove_element(list, "d");
    assert(adt_list_size(list) == 3);

    adt_list_reset(list);
    assert(adt_list_has_next(list));
    char* a = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* b = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* c = adt_list_get_next(list);
    assert(!adt_list_has_next(list));
    assert(adt_list_size(list) == 3);

    assert(!strcmp(a, "a"));
    assert(!strcmp(b, "b"));
    assert(!strcmp(c, "c"));

    adt_list_destroy(list);
    END_TEST();
}

void test_list_remove_element_middle(void) {
    BEGIN_TEST("list remove element middle");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "a");
    adt_list_add_back(list, "b");
    adt_list_add_back(list, "c");
    adt_list_add_back(list, "d");
    adt_list_add_back(list, "e");

    assert(adt_list_size(list) == 5);

    bool res1 = adt_list_remove_element(list, "b");
    assert(adt_list_size(list) == 4);

    bool res2 = adt_list_remove_element(list, "d");
    assert(adt_list_size(list) == 3);

    assert(res1 && res2);

    adt_list_reset(list);
    assert(adt_list_has_next(list));
    char* a = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* b = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* c = adt_list_get_next(list);
    assert(!adt_list_has_next(list));
    assert(adt_list_size(list) == 3);

    assert(!strcmp(a, "a"));
    assert(!strcmp(b, "c"));
    assert(!strcmp(c, "e"));

    adt_list_destroy(list);
    END_TEST();
}


void test_list_remove_element_only(void) {
    BEGIN_TEST("list remove element only");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");

    assert(adt_list_size(list) == 1);

    adt_list_remove_element(list, "1");
    assert(adt_list_size(list) == 0);
    adt_list_reset(list);
    assert(!adt_list_has_next(list));

    adt_list_destroy(list);
    END_TEST();
}

void test_list_remove_element_non_existent(void) {
    BEGIN_TEST("list remove element non existent");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");
    adt_list_add_back(list, "3");

    assert(adt_list_size(list) == 3);

    bool res = adt_list_remove_element(list, "4");
    assert(!res);
    assert(adt_list_size(list) == 3);

    adt_list_reset(list);
    assert(adt_list_has_next(list));
    char* a = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* b = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* c = adt_list_get_next(list);
    assert(!adt_list_has_next(list));

    assert(!strcmp(a, "1"));
    assert(!strcmp(b, "2"));
    assert(!strcmp(c, "3"));

    adt_list_destroy(list);
    END_TEST();
}

void test_reset(void) {
    BEGIN_TEST("reset");

    struct adt_list* list = adt_list_create();
    adt_list_add_back(list, "1");
    adt_list_add_back(list, "2");

    assert(adt_list_size(list) == 2);

    adt_list_reset(list);

    assert(adt_list_has_next(list));
    char* a = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* b = adt_list_get_next(list);
    assert(!adt_list_has_next(list));

    assert(!strcmp(a, "1"));
    assert(!strcmp(b, "2"));

    adt_list_reset(list);
    assert(adt_list_has_next(list));
    char* c = adt_list_get_next(list);
    assert(adt_list_has_next(list));
    char* d = adt_list_get_next(list);
    assert(!adt_list_has_next(list));

    assert(!strcmp(c, "1"));
    assert(!strcmp(d, "2"));

    adt_list_destroy(list);
    END_TEST();
}

void test_adt_list(void) {
    test_basic_insert_back_and_iter();
    test_basic_insert_front_and_iter();
    test_basic_get_head_and_tail();
    test_circulate();
    test_insert_alternating();
    test_insert_back_updates_tail_correctly();
    test_insert_front_updates_head_correctly();
    test_remove_back_updates_tail_correctly();
    test_remove_front_updates_head_correctly();
    test_remove_both_ends();
    test_remove_final_via_back();
    test_remove_final_via_front();
    test_add_first_via_front();
    test_add_first_via_back();
    test_reset();
    test_list_remove_element_middle();
    test_list_remove_element_first();
    test_list_remove_element_last();
    test_list_remove_element_only();
    test_list_remove_element_non_existent();    
}
