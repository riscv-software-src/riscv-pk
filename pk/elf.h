// See LICENSE for license details.

#ifndef _ELF_H
#define _ELF_H

#include <stdint.h>

#define IS_ELF(hdr) \
  ((hdr).e_ident[0] == 0x7f && (hdr).e_ident[1] == 'E' && \
   (hdr).e_ident[2] == 'L'  && (hdr).e_ident[3] == 'F')

#define IS_ELF32(hdr) (IS_ELF(hdr) && (hdr).e_ident[4] == 1)
#define IS_ELF64(hdr) (IS_ELF(hdr) && (hdr).e_ident[4] == 2)

#if __riscv_xlen == 64
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
# define Elf_Note Elf64_Note
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
# define Elf_Note Elf32_Note
#endif

#define ET_EXEC 2
#define ET_DYN 3

#define EF_RISCV_RVC 1

#define PT_LOAD 1
#define PT_INTERP 3
#define PT_LOOS 0x60000000
#define PT_GNU_PROPERTY (PT_LOOS + 0x474e553)
#define GNU_PROPERTY_TYPE_0_NAME "GNU"
#define NOTE_NAME_SZ (sizeof(GNU_PROPERTY_TYPE_0_NAME))

#define NT_GNU_PROPERTY_TYPE_0 5
#define GNU_PROPERTY_ALIGN 4

#define GUN_PROPERTY_RISCV_FEATURE_1_AND 0xc0000000
#define GNU_PROPERTY_RISCV_FEATURE_1_ZICFILP (1 << 0)
#define GNU_PROPERTY_RISCV_FEATURE_1_ZICFISS (1 << 1)

#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_ENTRY  9
#define AT_SECURE 23
#define AT_RANDOM 25

#define PF_X 1
#define PF_W 2
#define PF_R 4

typedef struct {
  uint8_t  e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
  uint32_t sh_name;
  uint32_t sh_type;
  uint32_t sh_flags;
  uint32_t sh_addr;
  uint32_t sh_offset;
  uint32_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint32_t sh_addralign;
  uint32_t sh_entsize;
} Elf32_Shdr;

typedef struct
{
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
} Elf32_Phdr;

typedef struct
{
  uint32_t st_name;
  uint32_t st_value;
  uint32_t st_size;
  uint8_t  st_info;
  uint8_t  st_other;
  uint16_t st_shndx;
} Elf32_Sym;

typedef struct {
  uint8_t  e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  uint32_t sh_name;
  uint32_t sh_type;
  uint64_t sh_flags;
  uint64_t sh_addr;
  uint64_t sh_offset;
  uint64_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint64_t sh_addralign;
  uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
} Elf64_Phdr;

typedef struct {
  uint32_t st_name;
  uint8_t  st_info;
  uint8_t  st_other;
  uint16_t st_shndx;
  uint64_t st_value;
  uint64_t st_size;
} Elf64_Sym;

typedef struct {
  uint32_t n_namesz;
  uint32_t n_descsz;
  uint32_t n_type;
} Elf32_Note;

typedef struct {
  uint32_t n_namesz;
  uint32_t n_descsz;
  uint32_t n_type;
} Elf64_Note;

struct gnu_property {
  uint32_t pr_type;
  uint32_t pr_datasz;
};

#endif
