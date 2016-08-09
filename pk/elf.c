// See LICENSE for license details.

#include "mmap.h"
#include "pk.h"
#include "mtrap.h"
#include "boot.h"
#include "bits.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>
#include <string.h>

void load_elf(const char* fn, elf_info* info)
{
  file_t* file = file_open(fn, O_RDONLY, 0);
  if (IS_ERR_VALUE(file))
    goto fail;

  Elf_Ehdr eh;
  ssize_t ehdr_size = file_pread(file, &eh, sizeof(eh), 0);
  if (ehdr_size < (ssize_t)sizeof(eh) ||
      !(eh.e_ident[0] == '\177' && eh.e_ident[1] == 'E' &&
        eh.e_ident[2] == 'L'    && eh.e_ident[3] == 'F'))
    goto fail;

#ifdef __riscv64
  assert(IS_ELF64(eh));
#else
  assert(IS_ELF32(eh));
#endif

#ifndef __riscv_compressed
  assert(!(eh.e_flags & EF_RISCV_RVC));
#endif

  uintptr_t min_vaddr = -1;
  size_t phdr_size = eh.e_phnum * sizeof(Elf_Phdr);
  if (phdr_size > info->phdr_size)
    goto fail;
  ssize_t ret = file_pread(file, (void*)info->phdr, phdr_size, eh.e_phoff);
  if (ret < (ssize_t)phdr_size)
    goto fail;
  info->phnum = eh.e_phnum;
  info->phent = sizeof(Elf_Phdr);
  Elf_Phdr* ph = (typeof(ph))info->phdr;
  for (int i = 0; i < eh.e_phnum; i++)
    if (ph[i].p_type == PT_LOAD && ph[i].p_memsz && ph[i].p_vaddr < min_vaddr)
      min_vaddr = ph[i].p_vaddr;
  min_vaddr = ROUNDDOWN(min_vaddr, RISCV_PGSIZE);
  uintptr_t bias = 0;
  if (eh.e_type == ET_DYN)
    bias = first_free_paddr - min_vaddr;
  min_vaddr += bias;
  info->entry = eh.e_entry + bias;
  int flags = MAP_FIXED | MAP_PRIVATE;
  for (int i = eh.e_phnum - 1; i >= 0; i--) {
    if(ph[i].p_type == PT_LOAD && ph[i].p_memsz) {
      uintptr_t prepad = ph[i].p_vaddr % RISCV_PGSIZE;
      uintptr_t vaddr = ph[i].p_vaddr + bias;
      if (vaddr + ph[i].p_memsz > info->brk_min)
        info->brk_min = vaddr + ph[i].p_memsz;
      int flags2 = flags | (prepad ? MAP_POPULATE : 0);
      if (__do_mmap(vaddr - prepad, ph[i].p_filesz + prepad, -1, flags2, file, ph[i].p_offset - prepad) != vaddr - prepad)
        goto fail;
      memset((void*)vaddr - prepad, 0, prepad);
      size_t mapped = ROUNDUP(ph[i].p_filesz + prepad, RISCV_PGSIZE) - prepad;
      if (ph[i].p_memsz > mapped)
        if (__do_mmap(vaddr + mapped, ph[i].p_memsz - mapped, -1, flags|MAP_ANONYMOUS, 0, 0) != vaddr + mapped)
          goto fail;
    }
  }

  file_decref(file);
  return;

fail:
  panic("couldn't open ELF program: %s!", fn);
}
