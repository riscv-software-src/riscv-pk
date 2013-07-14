#ifndef _VM_H
#define _VM_H

#include "syscall.h"
#include "file.h"
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

#define MAP_PRIVATE 0x2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_POPULATE 0x8000

void vm_init();
int handle_page_fault(uintptr_t vaddr, int prot);
void populate_mapping(const void* start, size_t size, int prot);
uintptr_t __do_mmap(uintptr_t addr, size_t length, int prot, int flags, file_t* file, off_t offset);
sysret_t do_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset);
sysret_t do_brk(uintptr_t addr);

#endif
