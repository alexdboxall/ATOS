
#include <string.h>
#include <assert.h>
#include <kprintf.h>
#include <errno.h>
#include <test.h>

extern int vfs_get_path_component(const char* path, int* ptr, char* output_buffer, int max_output_length, char delimiter);

static void test_standard_root_lookup(void) {
    BEGIN_TEST("finding the root drive of a path");

    int path_ptr = 0;
	char component_buffer[16];
	int status = vfs_get_path_component("con:", &path_ptr, component_buffer, 16, ':');

    assert(!strcmp(component_buffer, "con"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("con:"));

    END_TEST();
}

static void test_standard(void) {
    BEGIN_TEST("splitting file paths");

    int path_ptr = strlen("raw-hd0:first/");
	char component_buffer[16];
	int status = vfs_get_path_component("raw-hd0:first/second/third", &path_ptr, component_buffer, 16, '/');

    assert(!strcmp(component_buffer, "second"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("raw-hd0:first/second/"));

    END_TEST();
}

static void test_standard_with_multiple_slashes(void) {
    BEGIN_TEST("splitting file paths with multiple slashes");

    int path_ptr = strlen("raw-hd0:first///");
	char component_buffer[16];
	int status = vfs_get_path_component("raw-hd0:first///second/////third", &path_ptr, component_buffer, 16, '/');

    assert(!strcmp(component_buffer, "second"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("raw-hd0:first///second/////"));

    END_TEST();
}

static void test_bad_colon_in_path(void) {
    BEGIN_TEST("colons in filenames");

    int path_ptr = strlen("hd0:test/");
	char component_buffer[16];
	int status = vfs_get_path_component("hd0:test/bad:path/here", &path_ptr, component_buffer, 16, '/');
   
    assert(status == EINVAL);

    END_TEST();
}

static void test_bad_backslash_in_path(void) {
    BEGIN_TEST("backslashes in filenames");

    int path_ptr = strlen("hd0:test/");
	char component_buffer[16];
	int status = vfs_get_path_component("hd0:test/bad\\path/here", &path_ptr, component_buffer, 16, '/');
   
    assert(status == EINVAL);

    END_TEST();
}

static void test_almost_long_name(void) {
    BEGIN_TEST("long filenames (1)");

    int path_ptr = 0;
	char component_buffer[16];
	int status = vfs_get_path_component("123456789012345", &path_ptr, component_buffer, 16, '/');
   
    assert(!strcmp(component_buffer, "123456789012345"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("123456789012345"));

    END_TEST();
}

static void test_almost_long_name_but_with_limit_of_15(void) {
    BEGIN_TEST("long filenames (2)");
    
    int path_ptr = 0;
	char component_buffer[16];
	int status = vfs_get_path_component("123456789012345", &path_ptr, component_buffer, 15, '/');
   
    assert(status == ENAMETOOLONG);

    END_TEST();
}

static void test_almost_long_name_with_long_slashes(void) {
    BEGIN_TEST("long filenames (3)");

    int path_ptr = 0;
	char component_buffer[16];
	int status = vfs_get_path_component("123456789012345////////", &path_ptr, component_buffer, 16, '/');
   
    assert(!strcmp(component_buffer, "123456789012345"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("123456789012345////////"));

    END_TEST();
}

static void test_long_name(void) {
    BEGIN_TEST("long filenames (4)");

    int path_ptr = 0;
	char component_buffer[16];
	int status = vfs_get_path_component("1234567890123456", &path_ptr, component_buffer, 16, '/');
   
    assert(status == ENAMETOOLONG);

    END_TEST();
}

static void test_end_path(void) {
    BEGIN_TEST("finding the last file in a path");

    int path_ptr = strlen("start/");
	char component_buffer[16];
	int status = vfs_get_path_component("start/end", &path_ptr, component_buffer, 16, '/');
   
    assert(!strcmp(component_buffer, "end"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("start/end"));

    END_TEST();
}

static void test_end_path_with_trailing(void) {
    BEGIN_TEST("finding the last file in a path with a trailing slash");

    int path_ptr = strlen("start/");
	char component_buffer[16];
	int status = vfs_get_path_component("start/end//", &path_ptr, component_buffer, 16, '/');
   
    assert(!strcmp(component_buffer, "end"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("start/end//"));

    END_TEST();
}

static void test_slash_after_colon(void) {
    BEGIN_TEST("finding the root drive with a slash after the colon");

    int path_ptr = 0;
	char component_buffer[16];
	int status = vfs_get_path_component("con:///next/test", &path_ptr, component_buffer, 16, ':');

    assert(!strcmp(component_buffer, "con"));
    assert(status == 0);
    assert(path_ptr == (int) strlen("con:///"));

    END_TEST();
}

static void test_multiple(void) {
    BEGIN_TEST("finding each segment of a filepath");

    int path_ptr = 0;

    char component_buffer[16];
	int status = vfs_get_path_component("1:/2/3//4/", &path_ptr, component_buffer, 16, ':');

    assert(!strcmp(component_buffer, "1"));
    assert(status == 0);

    vfs_get_path_component("1:/2/3//4/", &path_ptr, component_buffer, 16, '/');
    assert(!strcmp(component_buffer, "2"));
    assert(status == 0);

    vfs_get_path_component("1:/2/3//4/", &path_ptr, component_buffer, 16, '/');
    assert(!strcmp(component_buffer, "3"));
    assert(status == 0);

    vfs_get_path_component("1:/2/3//4/", &path_ptr, component_buffer, 16, '/');
    assert(!strcmp(component_buffer, "4"));
    assert(status == 0);

    END_TEST();
}

void test_vfs_get_path_component(void) {
    test_standard_root_lookup();
    test_standard();
    test_standard_with_multiple_slashes();
    test_bad_colon_in_path();
    test_bad_backslash_in_path();
    test_almost_long_name();
    test_almost_long_name_but_with_limit_of_15();
    test_almost_long_name_with_long_slashes();
    test_long_name();
    test_end_path_with_trailing();
    test_end_path();
    test_slash_after_colon();
    test_multiple();
}