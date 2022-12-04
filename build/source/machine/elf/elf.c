#include <machine/elf.h>
#include <machine/symbols.h>
#include <errno.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>
#include <arch.h>
#include <virtual.h>
#include <physical.h>

bool is_elf_valid(struct Elf32_Ehdr* header) {
    /*
    * Ensure the signature is correct.
    */
    if (header->e_ident[EI_MAG0] != ELFMAG0) return false;
    if (header->e_ident[EI_MAG1] != ELFMAG1) return false;
    if (header->e_ident[EI_MAG2] != ELFMAG2) return false;
    if (header->e_ident[EI_MAG3] != ELFMAG3) return false;

    /* TODO: check for other things, such as the platform, etc. */

    return true;
}

static void* addToVoidPointer(void* p, size_t o) {
    return (void*) (((uint8_t*) p) + o);
}

static size_t elf_load_program_headers(void* data, size_t relocation_point, bool relocate) {

    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr* prog_headers = (struct Elf32_Phdr*) addToVoidPointer(data, elf_header->e_phoff);

    size_t base_point = relocate ? 0xD0000000U : 0x10000000;
    size_t sbrk_address = base_point;

    for (int i = 0; i < elf_header->e_phnum; ++i) {
        struct Elf32_Phdr* prog_header = prog_headers + i;

        size_t address = prog_header->p_vaddr;
		size_t offset = prog_header->p_offset;
		size_t size = prog_header->p_filesz;
		size_t num_zero_bytes = prog_header->p_memsz - size;
		size_t type = prog_header->p_type;

		if (type == PHT_LOAD) {
			if (!relocate) {
                size_t num_pages = virt_bytes_to_pages(num_zero_bytes);
                size_t num_zero_pages = virt_bytes_to_pages(num_zero_bytes);
                for (size_t i = 0; i < num_pages; ++i) {
                    vas_map(vas_get_current_vas(), phys_allocate(), address + (i * ARCH_PAGE_SIZE), VAS_FLAG_USER | VAS_FLAG_WRITABLE | VAS_FLAG_PRESENT);
                }

                memcpy((void*) address, (const void*) addToVoidPointer(data, offset), size);

                for (size_t i = 0; i < num_zero_pages; ++i) {
                    kprintf("TODO: assert that we are in the right VAS() -> program loading should be done in the already-in-usermode-allocate-a-stack startup function");
                    vas_reflag(vas_get_current_vas(), address + size + (i * ARCH_PAGE_SIZE), VAS_FLAG_USER | VAS_FLAG_WRITABLE | VAS_FLAG_ALLOCATE_ON_ACCESS);
                }

                if (address + size > sbrk_address) {
                    sbrk_address = address + size;
                }
            
			} else {
				memcpy((void*) (address + relocation_point - base_point), (const void*) addToVoidPointer(data, offset), size);
				memset((void*) (address + relocation_point - base_point + size), 0, num_zero_bytes);
			}
		}
    }

    return sbrk_address;
}

static char* elf_lookup_string(void* data, int offset) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;

	if (elf_header->e_shstrndx == SHN_UNDEF) {
		return NULL;
	}

	struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) addToVoidPointer(data, elf_header->e_shoff);

	char* string_table = (char*) addToVoidPointer(data, sect_headers[elf_header->e_shstrndx].sh_offset);
	if (string_table == NULL) {
		return NULL;
	}

	return string_table + offset;
}

static size_t elf_get_symbol_value(void* data, int table, size_t index, bool* error, size_t relocation_point, size_t base_address) {
    *error = false;

	if (table == SHN_UNDEF || index == SHN_UNDEF) {
		*error = true;
		return 0;
	}

    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
    struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) addToVoidPointer(data, elf_header->e_shoff);
	struct Elf32_Shdr* symbol_table = sect_headers + table;

	size_t num_symbol_table_entries = symbol_table->sh_size / symbol_table->sh_entsize;
	if (index >= num_symbol_table_entries) {
		*error = true;
		return 0;
	}

	struct Elf32_Sym* symbol = ((struct Elf32_Sym*) addToVoidPointer(data, symbol_table->sh_offset)) + index;

	if (symbol->st_shndx == SHN_UNDEF) {

		// external symbol (i.e. kernel symbol)

		struct Elf32_Shdr* string_table = sect_headers + symbol_table->sh_link;
		const char* name = (const char*) addToVoidPointer(data, string_table->sh_offset + symbol->st_name);

		size_t target = ksymbol_get_address_from_name(name);

		if (target == 0) {
			if (!(ELF32_ST_BIND(symbol->st_info) & STB_WEAK)) {
				*error = true;
			}

            kprintf("missing symbol: %s\n", name);

			return 0;

		} else {
			return target;
		}

	} else if (symbol->st_shndx == SHN_ABS) {
		return symbol->st_value;

	} else {
		return symbol->st_value + (relocation_point - base_address);
	}
}

static bool elf_perform_relocation(void* data, size_t relocation_point, struct Elf32_Shdr* section, struct Elf32_Rel* relocation_table)
{
	size_t base_address = 0xD0000000U;

	size_t addr = (size_t) relocation_point - base_address + relocation_table->r_offset;
	size_t* ref = (size_t*) addr;

	int symbolValue = 0;
	if (ELF32_R_SYM(relocation_table->r_info) != SHN_UNDEF) {
		bool error = false;
		symbolValue = elf_get_symbol_value(data, section->sh_link, ELF32_R_SYM(relocation_table->r_info), &error, relocation_point, base_address);
		if (error) {
			return false;
		}
	}
	
	int type = ELF32_R_TYPE(relocation_table->r_info);
	if (type == R_386_32) {
		*ref = DO_386_32(symbolValue, *ref);

	} else if (type == R_386_PC32) {
		*ref = DO_386_PC32(symbolValue, *ref, (size_t) ref);

	} else if (type == R_386_RELATIVE) {
		*ref = DO_386_RELATIVE((relocation_point - base_address), *ref);

	} else {
		return false;
	}

	return true;
}

static bool elf_perform_relocations(void* data, size_t relocation_point) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) addToVoidPointer(data, elf_header->e_shoff);

	for (int i = 0; i < elf_header->e_shnum; ++i) {
		struct Elf32_Shdr* section = sect_headers + i;

		if (section->sh_type == SHT_REL) {
			struct Elf32_Rel* relocation_tables = (struct Elf32_Rel*) addToVoidPointer(data, section->sh_offset);
			int count = section->sh_size / section->sh_entsize;

			if (strcmp(elf_lookup_string(data, section->sh_name), ".rel.dyn")) {
				continue;
			}

			for (int index = 0; index < count; ++index) {
				bool success = elf_perform_relocation(data, relocation_point, section, relocation_tables + index);
				if (!success) {
					return false;
				}
			}


		} else if (section->sh_type == SHT_RELA) {
			panic("unsupported section type: SHT_RELA");
		}
	}

	return true;
}

static size_t elf_load(void* data, size_t relocation_point, bool relocate) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;

    /*
    * Check the file is a valid ELF file.
    */
    if (!is_elf_valid(elf_header)) {
        return 0;
    }

    /*
    * To load a driver, we need the section headers
    */
    if (elf_header->e_shnum == 0 && relocate) {
        return 0;
    }

    /*
    * We always need the program headers.
    */
    if (elf_header->e_phnum == 0) {
        return 0;
    }

    /*
    * Load into memory.
    */
    size_t result = elf_load_program_headers(data, relocation_point, relocate);

    if (relocate) {
        bool success = elf_perform_relocations(data, relocation_point);
        if (success) {
            return elf_header->e_entry - 0xD0000000U + relocation_point;
        } else {
            return 0;
        }
    }

    return result;
}

size_t arch_load_driver(void* data, size_t data_size, size_t relocation_point) {
    (void) data_size;

    /* Zero is returned on error. */
    size_t result = elf_load(data, relocation_point, true);

    return result;
}

int arch_start_driver(size_t driver, void* argument) {
    if (driver == 0) {
        return EINVAL;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address"
    void (*execution_address)(void*) = (void (*)(void*)) driver;
#pragma GCC diagnostic pop

    execution_address(argument);

    return 0;
}