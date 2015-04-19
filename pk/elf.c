// See LICENSE for license details.

#include "file.h"
#include "pk.h"
#include "vm.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>
#include <string.h>

void load_elf(const char* fn, elf_info* info)
{
  file_t* file = file_open(fn, O_RDONLY, 0);
  if (IS_ERR_VALUE(file))
    goto fail;

  Elf64_Ehdr eh64;
  ssize_t ehdr_size = file_pread(file, &eh64, sizeof(eh64), 0);
  if (ehdr_size < (ssize_t)sizeof(eh64) ||
      !(eh64.e_ident[0] == '\177' && eh64.e_ident[1] == 'E' &&
        eh64.e_ident[2] == 'L'    && eh64.e_ident[3] == 'F'))
    goto fail;

  uintptr_t min_vaddr = -1, max_vaddr = 0;

  #define LOAD_ELF do { \
    eh = (typeof(eh))&eh64; \
    size_t phdr_size = eh->e_phnum*sizeof(*ph); \
    if (phdr_size > info->phdr_size) \
      goto fail; \
    ssize_t ret = file_pread(file, (void*)info->phdr, phdr_size, eh->e_phoff); \
    if (ret < (ssize_t)phdr_size) \
      goto fail; \
    info->phnum = eh->e_phnum; \
    info->phent = sizeof(*ph); \
    ph = (typeof(ph))info->phdr; \
    info->is_supervisor = (eh->e_entry >> (8*sizeof(eh->e_entry)-1)) != 0; \
    if (info->is_supervisor) \
      info->first_free_paddr = ROUNDUP(info->first_free_paddr, SUPERPAGE_SIZE); \
    for (int i = 0; i < eh->e_phnum; i++) \
      if (ph[i].p_type == PT_LOAD && ph[i].p_memsz && ph[i].p_vaddr < min_vaddr) \
        min_vaddr = ph[i].p_vaddr; \
    if (info->is_supervisor) \
      min_vaddr = ROUNDDOWN(min_vaddr, SUPERPAGE_SIZE); \
    else \
      min_vaddr = ROUNDDOWN(min_vaddr, RISCV_PGSIZE); \
    uintptr_t bias = 0; \
    if (info->is_supervisor || eh->e_type == ET_DYN) \
      bias = info->first_free_paddr - min_vaddr; \
    info->entry = eh->e_entry; \
    if (!info->is_supervisor) { \
      info->entry += bias; \
      min_vaddr += bias; \
    } \
    info->bias = bias; \
    int flags = MAP_FIXED | MAP_PRIVATE; \
    if (info->is_supervisor) \
      flags |= MAP_POPULATE; \
    for (int i = eh->e_phnum - 1; i >= 0; i--) { \
      if(ph[i].p_type == PT_LOAD && ph[i].p_memsz) { \
        uintptr_t prepad = ph[i].p_vaddr % RISCV_PGSIZE; \
        uintptr_t vaddr = ph[i].p_vaddr + bias; \
        if (vaddr + ph[i].p_memsz > max_vaddr) \
          max_vaddr = vaddr + ph[i].p_memsz; \
        if (info->is_supervisor) { \
          if (!__valid_user_range(vaddr - prepad, vaddr + ph[i].p_memsz)) \
            goto fail; \
          ret = file_pread(file, (void*)vaddr, ph[i].p_filesz, ph[i].p_offset); \
          if (ret < (ssize_t)ph[i].p_filesz) \
            goto fail; \
          memset((void*)vaddr - prepad, 0, prepad); \
          memset((void*)vaddr + ph[i].p_filesz, 0, ph[i].p_memsz - ph[i].p_filesz); \
        } else { \
          int flags2 = flags | (prepad ? MAP_POPULATE : 0); \
          if (__do_mmap(vaddr - prepad, ph[i].p_filesz + prepad, -1, flags2, file, ph[i].p_offset - prepad) != vaddr - prepad) \
            goto fail; \
          memset((void*)vaddr - prepad, 0, prepad); \
          size_t mapped = ROUNDUP(ph[i].p_filesz + prepad, RISCV_PGSIZE) - prepad; \
          if (ph[i].p_memsz > mapped) \
            if (__do_mmap(vaddr + mapped, ph[i].p_memsz - mapped, -1, flags|MAP_ANONYMOUS, 0, 0) != vaddr + mapped) \
              goto fail; \
        } \
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

  info->first_user_vaddr = min_vaddr;
  info->first_vaddr_after_user = ROUNDUP(max_vaddr - info->bias, RISCV_PGSIZE);
  info->brk_min = max_vaddr;

  file_decref(file);
  return;

fail:
    panic("couldn't open ELF program: %s!", fn);
}
