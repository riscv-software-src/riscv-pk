#ifndef _VM_H
#define _VM_H

#include "syscall.h"
#include "file.h"
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#define SUPERPAGE_SIZE ((uintptr_t)(RISCV_PGSIZE << RISCV_PGLEVEL_BITS))
#ifdef __riscv64
# define VM_CHOICE VM_SV39
# define VA_BITS 39
# define MEGAPAGE_SIZE (SUPERPAGE_SIZE << RISCV_PGLEVEL_BITS)
#else
# define VM_CHOICE VM_SV32
# define VA_BITS 32
#endif

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

#define MAP_PRIVATE 0x2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_POPULATE 0x8000
#define MREMAP_FIXED 0x2

#define supervisor_paddr_valid(start, length) \
  ((uintptr_t)(start) >= current.first_user_vaddr + current.bias \
   && (uintptr_t)(start) + (length) < mem_size \
   && (uintptr_t)(start) + (length) >= (uintptr_t)(start))

void vm_init();
void supervisor_vm_init();
uintptr_t pk_vm_init();
int handle_page_fault(uintptr_t vaddr, int prot);
void populate_mapping(const void* start, size_t size, int prot);
void __map_kernel_range(uintptr_t va, uintptr_t pa, size_t len, int prot);
int __valid_user_range(uintptr_t vaddr, size_t len);
uintptr_t __do_mmap(uintptr_t addr, size_t length, int prot, int flags, file_t* file, off_t offset);
uintptr_t do_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset);
int do_munmap(uintptr_t addr, size_t length);
uintptr_t do_mremap(uintptr_t addr, size_t old_size, size_t new_size, int flags);
uintptr_t do_mprotect(uintptr_t addr, size_t length, int prot);
uintptr_t do_brk(uintptr_t addr);

typedef uintptr_t pte_t;
extern pte_t* root_page_table;

static inline void flush_tlb()
{
  asm volatile("sfence.vm");
}

#endif
