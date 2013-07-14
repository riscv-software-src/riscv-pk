// See LICENSE for license details.

#include "file.h"
#include "pk.h"
#include "pcr.h"
#include "vm.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>
#include <string.h>

void load_elf(const char* fn, elf_info* info)
{
  sysret_t ret = file_open(fn, O_RDONLY, 0);
  file_t* file = (file_t*)ret.result;
  if (ret.result == -1)
    goto fail;

  Elf64_Ehdr eh64;
  ssize_t ehdr_size = file_pread(file, &eh64, sizeof(eh64), 0).result;
  if (ehdr_size < (ssize_t)sizeof(eh64) ||
      !(eh64.e_ident[0] == '\177' && eh64.e_ident[1] == 'E' &&
        eh64.e_ident[2] == 'L'    && eh64.e_ident[3] == 'F'))
    goto fail;

  #define LOAD_ELF do { \
    eh = (typeof(eh))&eh64; \
    size_t phdr_size = eh->e_phnum*sizeof(*ph); \
    if (info->phdr_top - phdr_size < info->stack_bottom) \
      goto fail; \
    info->phdr = info->phdr_top - phdr_size; \
    ssize_t ret = file_pread(file, (void*)info->phdr, phdr_size, eh->e_phoff).result; \
    if (ret < (ssize_t)phdr_size) goto fail; \
    info->entry = eh->e_entry; \
    info->phnum = eh->e_phnum; \
    info->phent = sizeof(*ph); \
    ph = (typeof(ph))info->phdr; \
    for(int i = 0; i < eh->e_phnum; i++, ph++) { \
      if(ph->p_type == SHT_PROGBITS && ph->p_memsz) { \
        info->brk_min = MAX(info->brk_min, ph->p_vaddr + ph->p_memsz); \
        size_t vaddr = ROUNDDOWN(ph->p_vaddr, RISCV_PGSIZE), prepad = ph->p_vaddr - vaddr; \
        size_t memsz = ph->p_memsz + prepad, filesz = ph->p_filesz + prepad; \
        size_t offset = ph->p_offset - prepad; \
        if (__do_mmap(vaddr, filesz, -1, MAP_FIXED|MAP_PRIVATE, file, offset) != vaddr) \
          goto fail; \
        size_t mapped = ROUNDUP(filesz, RISCV_PGSIZE); \
        if (memsz > mapped) \
          if (__do_mmap(vaddr + mapped, memsz - mapped, -1, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0, 0) != vaddr + mapped) \
            goto fail; \
      } \
    } \
  } while(0)

  info->elf64 = IS_ELF64(eh64);
  if (info->elf64)
  {
    Elf64_Ehdr* eh;
    Elf64_Phdr* ph;
    LOAD_ELF;
  }
  else if (IS_ELF32(eh64))
  {
    Elf32_Ehdr* eh;
    Elf32_Phdr* ph;
    LOAD_ELF;
  }
  else
    goto fail;

  file_decref(file);
  return;

fail:
    panic("couldn't open ELF program: %s!", fn);
}
