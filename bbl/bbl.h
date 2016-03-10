// See LICENSE for license details.

#ifndef _BBL_H
#define _BBL_H

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <stddef.h>

typedef struct {
  uintptr_t entry;
  uintptr_t first_user_vaddr;
  uintptr_t first_vaddr_after_user;
  uintptr_t load_offset;
} kernel_elf_info;

void load_kernel_elf(void* blob, size_t size, kernel_elf_info* info);
void print_logo();

#endif // !__ASSEMBLER__

#endif
