#include <machine/symbols.h>
#include <machine/elf.h>
#include <stdbool.h>
#include <adt.h>
#include <vfs.h>
#include <kprintf.h>
#include <panic.h>
#include <fcntl.h>
#include <string.h>
#include <heap.h>

struct adt_hashmap* loaded_symbols = NULL;

static void ksymbol_init(void) {
    loaded_symbols = adt_hashmap_create(512, adt_hashmap_null_terminated_string_hash_function);

    /*
    * Open the kernel file so we can read its symbol table.
    */
    struct vnode* file;
    int ret = vfs_open("hd0:/System/kernel.exe", O_RDONLY, 0, &file);
    if (ret != 0) {
        goto fail;
    }
        
    /*
    * Read the ELF header.
    *
    * If we 'goto fail', this doesn't get freed - but failure ends up calling panic()
    * so it doesn't matter. The same goes for the various other things that get allocated.
    */
    struct Elf32_Ehdr* elf_header = malloc(sizeof(struct Elf32_Ehdr));

    struct uio uio = uio_construct_read(elf_header, sizeof(struct Elf32_Ehdr), 0);
    ret = vfs_read(file, &uio);
    if (ret != 0) {
        goto fail;
    }

    /*
    * These should never happen - otherwise our kernel shouldn't be running!
    */
    if (!is_elf_valid(elf_header) || elf_header->e_shoff == 0) {
        goto fail;
    }

    /*
    * Load the section headers.
    */
    struct Elf32_Shdr* section_headers = malloc(elf_header->e_shnum * elf_header->e_shentsize);
    uio.address = section_headers;
    uio.offset = elf_header->e_shoff;
    uio.length_remaining = elf_header->e_shnum * elf_header->e_shentsize;
    ret = vfs_read(file, &uio);
    if (ret != 0) {
        goto fail;
    }

    size_t symbol_table_offset = 0;
    size_t symbol_table_length = 0;
    size_t string_table_offset = 0;
    size_t string_table_length = 0;

    /*
    * Find the address and size of the symbol and string tables.
    */
    for (int i = 0; i < elf_header->e_shnum; ++i) {
        size_t file_offset = (section_headers + i)->sh_offset;
        size_t address = (section_headers + elf_header->e_shstrndx)->sh_offset + (section_headers + i)->sh_name;

        char name_buffer[32];
        memset(name_buffer, 0, 32);

        uio.address = name_buffer;
        uio.offset = address;
        uio.length_remaining = 31;
        ret = vfs_read(file, &uio);
        if (ret != 0) {
            goto fail;
        }

        if (!strcmp(name_buffer, ".symtab")) {
            symbol_table_offset = file_offset;
            symbol_table_length = (section_headers + i)->sh_size;

        } else if (!strcmp(name_buffer, ".strtab")) {
            string_table_offset = file_offset;
            string_table_length = (section_headers + i)->sh_size;
        }
    }
    
    if (symbol_table_offset == 0 || string_table_offset == 0 || symbol_table_length == 0 || string_table_length == 0) {
        goto fail;
    }

    /*
    * Load the symbol table.
    */
    struct Elf32_Sym* symbol_table = malloc(symbol_table_length);
    uio = uio_construct_read(symbol_table, symbol_table_length, symbol_table_offset);
    ret = vfs_read(file, &uio);
    if (ret != 0) {
        goto fail;
    }

    /*
    * Load the string table.
    */
    const char* string_table = malloc(string_table_length);
    uio = uio_construct_read((void*) string_table, string_table_length, string_table_offset);
    ret = vfs_read(file, &uio);
    if (ret != 0) {
        goto fail;
    }

    /*
    * Register all of the symbols we find.
    */
    for (size_t i = 0; i < symbol_table_length / sizeof(struct Elf32_Sym); ++i) {
        struct Elf32_Sym symbol = symbol_table[i];

        if (symbol.st_value == 0) { 
            continue;
        }

        //kprintf("%d: about to load symbol @ offset %d (table size = %d,%d (str/sym) @ 0x%X,0x%X)\n", i, symbol.st_name, string_table_length, symbol_table_length, string_table_offset, symbol_table_offset);

        char* name = strdup(string_table + symbol.st_name);
        ksymbol_add_symbol(name, symbol.st_value);
    }

    /*
    * Cleanup.
    */
    free(symbol_table);
    free((void*) string_table);
    free(elf_header);
    free(section_headers);

    vfs_close(file);
    
    return;

fail:
    panic("ksymbol_init: kernel.exe could not be read or is invalid");
}

void ksymbol_add_symbol(const char* name, size_t address) {
    if (loaded_symbols == NULL) {
        ksymbol_init();
    }

    adt_hashmap_add(loaded_symbols, (void*) name, (void*) address);
}

size_t ksymbol_get_address_from_name(const char* name) {
    if (loaded_symbols == NULL) {
        ksymbol_init();
    }

    if (adt_hashmap_contains(loaded_symbols, (void*) name)) {
        return (size_t) adt_hashmap_get(loaded_symbols, (void*) name);
    }

    return 0;
}