
extern void test_vfs_get_path_component(void);
extern void test_adt_list(void);
extern void test_vfs_open_read(void);

void test_kernel(void)
{
	test_adt_list();
	test_vfs_get_path_component();
	test_vfs_open_read();
}