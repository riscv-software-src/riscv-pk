// See LICENSE for license details.

#ifndef _BBL_H
#define _BBL_H

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <stddef.h>

uintptr_t load_kernel_elf(void* blob, size_t size);
void print_logo();

#endif // !__ASSEMBLER__

#endif
