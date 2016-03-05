// See LICENSE for license details.

#ifndef _BOOT_H
#define _BOOT_H

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <stddef.h>

struct mainvars {
  uint64_t argc;
  uint64_t argv[127]; // this space is shared with the arg strings themselves
};

typedef struct {
  int phent;
  int phnum;
  int is_supervisor;
  size_t phdr;
  size_t phdr_size;
  size_t first_free_paddr;
  size_t first_user_vaddr;
  size_t first_vaddr_after_user;
  size_t bias;
  size_t entry;
  size_t brk_min;
  size_t brk;
  size_t brk_max;
  size_t mmap_max;
  size_t stack_bottom;
  size_t stack_top;
  size_t t0;
} elf_info;

extern elf_info current;

void prepare_supervisor_mode();
void run_loaded_program(struct mainvars*);
void boot_loader();
void boot_other_hart();
void load_elf(const char* fn, elf_info* info);
void print_logo();

#endif // !__ASSEMBLER__

#endif
