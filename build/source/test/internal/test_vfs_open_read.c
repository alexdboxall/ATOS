
#include <test.h>
#include <vfs.h>
#include <vnode.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <uio.h>

void test_open_file(void) {
    BEGIN_TEST("opening a file");

    struct vnode* node;
	int status;
    
    status = vfs_open("hd0:/test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);

    END_TEST();
}

void test_file_read(void) {
    BEGIN_TEST("reading a file");

    struct vnode* node;
    int status;
    char buffer[9];
    
    status = vfs_open("hd0:/test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);
    
    struct uio io = uio_construct_kernel_read(buffer, 9, 0);
    status = vfs_read(node, &io);

    assert(status == 0);
    assert(io.length_remaining == 0);
    assert(!memcmp(buffer, "ROOT FILE", 9));

    vfs_close(node);
    
    END_TEST();
}

void test_file_read_with_offset(void) {
    BEGIN_TEST("reading a file from an offset");

    /*
    * Also tests attempting reading past the end of a file
    * (when starting in a valid part of the file)
    */

    struct vnode* node;
    int status;
    char buffer[9];
    
    status = vfs_open("hd0:/test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);
    
    struct uio io = uio_construct_kernel_read(buffer, 20, 5);
    status = vfs_read(node, &io);

    assert(status == 0);
    assert(io.length_remaining == 16);
    assert(!memcmp(buffer, "FILE", 4));
    
    vfs_close(node);
    
    END_TEST();
}

void test_file_read_way_past_end(void) {
    BEGIN_TEST("reading a file past the end");
    
    struct vnode* node;
    int status;
    char buffer[9];
    
    status = vfs_open("hd0:/test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);

    buffer[0] = 'A';
    
    struct uio io = uio_construct_kernel_read(buffer, 5, 100);
    status = vfs_read(node, &io);
    
    assert(status == 0);
    assert(buffer[0] == 'A');
    assert(io.length_remaining == 5);
    
    vfs_close(node);
    
    END_TEST();
}

void test_file_read_fails_on_dir(void) {
    BEGIN_TEST("reading a directory with the file read function");

    struct vnode* node;
    int status;
    char buffer[9];

    status = vfs_open("hd0:/test/", O_RDONLY, 0, &node);
    assert(status == 0);

    buffer[0] = 'B';

    struct uio io = uio_construct_kernel_read(buffer, 9, 0);
    status = vfs_read(node, &io);

    assert(buffer[0] == 'B');
    assert(status == EISDIR);

    vfs_close(node);
    
    END_TEST();
}

void test_double_file_read(void) {
    BEGIN_TEST("reading a file twice");

    struct vnode* node;
    int status;
    char buffer[9];
    
    status = vfs_open("hd0:/test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);
    
    struct uio io = uio_construct_kernel_read(buffer, 5, 0);
    status = vfs_read(node, &io);
    assert(status == 0);
    assert(!memcmp(buffer, "ROOT ", 5));
    assert(io.length_remaining == 0);
    io.length_remaining += 4;
    status = vfs_read(node, &io);
    assert(status == 0);
    assert(!memcmp(buffer, "ROOT FILE", 9));
    assert(io.length_remaining == 0);
    
    vfs_close(node);
    
    END_TEST();
}

void test_open_with_dot_slash(void) {
    BEGIN_TEST("opening a file with a dot slash in the path (1)");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/././root file.txt/", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);

    END_TEST();
}


void test_open_with_dot_slash_2(void) {
    BEGIN_TEST("opening a file with a dot slash in the path (2)");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/././test/./root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);

    END_TEST();
}

void test_open_with_dot_slash_after_file(void) {
    BEGIN_TEST("opening a file with a dot slash after the path");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/root file.txt/./", O_RDONLY, 0, &node);
    assert(status == ENOTDIR);

    END_TEST();
}

void test_open_subdir_file(void) {
    BEGIN_TEST("opening a file in a subdirectory");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/subdir/sub file.txt", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);

    END_TEST();
}

void test_open_dir(void) {
    BEGIN_TEST("opening a directory");

    struct vnode* node;
	int status;
    
    status = vfs_open("hd0:/test/subdir", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);

    END_TEST();
}

void test_open_file_write(void) {
    BEGIN_TEST("opening a file for writing");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/root file.txt", O_WRONLY, 0, &node);
    assert(status == EROFS);

    END_TEST();
}

void test_open_dir_write(void) {
    BEGIN_TEST("opening a directory for writing");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/subdir", O_WRONLY, 0, &node);
    assert(status == EISDIR || status == EROFS);
    
    END_TEST();
}

void test_open_no_device(void) {
    BEGIN_TEST("opening a file with a non-existent device");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd123:/test/root file.txt", O_RDONLY, 0, &node);
    assert(status == ENODEV);
    
    END_TEST();
}

void test_open_no_file(void) {
    BEGIN_TEST("opening a file that doesn't exist");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/no file.txt", O_RDONLY, 0, &node);
    assert(status == ENOENT);
    
    END_TEST();
}

void test_open_file_in_middle_of_path(void) {
    BEGIN_TEST("opening a file in the middle of a path");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/root file.txt/subdir", O_RDONLY, 0, &node);
    assert(status == ENOTDIR);
    
    END_TEST();
}

void test_open_filesystem_root(void) {
    BEGIN_TEST("opening the filesystem root");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);
    
    END_TEST();
}

void test_open_going_back_1(void) {
    BEGIN_TEST("backtracking through a filepath twice (1)");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/../test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);
    
    END_TEST();
}

void test_open_going_back_2(void) {
    BEGIN_TEST("backtracking through a filepath once (2)");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/subdir/../root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);
    
    END_TEST();
}

void test_open_going_back_twice_1(void) {
    BEGIN_TEST("backtracking through a filepath twice (1)");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/subdir/../../test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);

    vfs_close(node);
    
    END_TEST();
}

void test_open_going_back_twice_2(void) {
    BEGIN_TEST("backtracking through a filepath twice (2)");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/subdir/../../test/../test/subdir/../", O_RDONLY, 0, &node);
    assert(status == 0);

    assert(vnode_op_dirent_type(node) == DT_DIR);

    vfs_close(node);
    
    END_TEST();
}

void test_open_going_back_past_root(void) {
    BEGIN_TEST("backtracking past the root");
    
    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/subdir/../../../../../../..", O_RDONLY, 0, &node);
    assert(status == 0);
    assert(vnode_op_dirent_type(node) == DT_DIR);
    vfs_close(node);

    status = vfs_open("hd0:/../", O_RDONLY, 0, &node);
    assert(status == 0);
    assert(vnode_op_dirent_type(node) == DT_DIR);
    vfs_close(node);

    status = vfs_open("hd0:/../../../../test/subdir/../../../", O_RDONLY, 0, &node);
    assert(status == 0);
    assert(vnode_op_dirent_type(node) == DT_DIR);
    vfs_close(node);

    status = vfs_open("hd0:/test/subdir/../../../../../../../test", O_RDONLY, 0, &node);
    assert(status == 0);
    assert(vnode_op_dirent_type(node) == DT_DIR);
    vfs_close(node);

    status = vfs_open("hd0:/test/subdir/../../../../../../../test/subdir/", O_RDONLY, 0, &node);
    assert(status == 0);
    assert(vnode_op_dirent_type(node) == DT_DIR);
    vfs_close(node);

    END_TEST();
}

void test_open_going_back_with_invalid(void) {
    BEGIN_TEST("backtracking through invalid filepath components");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/notdir/../", O_RDONLY, 0, &node);
    assert(status == ENOENT);

    status = vfs_open("hd0:/test/root file.txt", O_RDONLY, 0, &node);
    assert(status == 0);
    vfs_close(node);

    status = vfs_open("hd0:/test/notdir/../test/", O_RDONLY, 0, &node);
    assert(status == ENOENT);

    status = vfs_open("hd0:/test/subdir/a/../sub file.txt", O_RDONLY, 0, &node);
    assert(status == ENOENT);

    status = vfs_open("hd0:/test/subdir/sub file.txt", O_RDONLY, 0, &node);
    assert(status == 0);
    vfs_close(node);

    END_TEST();
}

void test_open_going_back_after_file(void) {
    BEGIN_TEST("backtracking after a file has been reached");

    struct vnode* node;
    int status;
    
    status = vfs_open("hd0:/test/root file.txt/../", O_RDONLY, 0, &node);
    assert(status == ENOTDIR);

    status = vfs_open("hd0:/test/../../test/root file.txt/../", O_RDONLY, 0, &node);
    assert(status == ENOTDIR);

    status = vfs_open("hd0:/test/../../test/root file.txt/", O_RDONLY, 0, &node);
    assert(status == 0);
    
    vfs_close(node);
    
    END_TEST();
}

void test_vfs_open_read(void) {
    test_open_file();
    test_file_read();
    test_file_read_with_offset();
    test_file_read_way_past_end();
    test_file_read_fails_on_dir();
    test_double_file_read();
    test_open_with_dot_slash();
    test_open_with_dot_slash_2();
    test_open_with_dot_slash_after_file();
    test_open_subdir_file();
    test_open_dir();
    test_open_file_write();
    test_open_dir_write();
    test_open_no_device();
    test_open_no_file();
    test_open_file_in_middle_of_path();
    test_open_filesystem_root();
    test_open_going_back_1();
    test_open_going_back_2();
    test_open_going_back_twice_1();
    test_open_going_back_twice_2();
    test_open_going_back_past_root();
    test_open_going_back_with_invalid();
    test_open_going_back_after_file();
}
