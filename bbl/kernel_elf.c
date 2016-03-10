// See LICENSE for license details.

#include "mtrap.h"
#include "bbl.h"
#include "bits.h"
#include "vm.h"
#include <elf.h>
#include <string.h>

void load_kernel_elf(void* blob, size_t size, kernel_elf_info* info)
{
  Elf_Ehdr* eh = blob;
  if (sizeof(*eh) > size ||
      !(eh->e_ident[0] == '\177' && eh->e_ident[1] == 'E' &&
        eh->e_ident[2] == 'L'    && eh->e_ident[3] == 'F'))
    goto fail;

  if (IS_ELF64(*eh) != (sizeof(uintptr_t) == 8))
    goto fail;

  uintptr_t min_vaddr = -1, max_vaddr = 0;
  size_t phdr_size = eh->e_phnum * sizeof(Elf_Ehdr);
  Elf_Phdr* ph = blob + eh->e_phoff;
  if (eh->e_phoff + phdr_size > size)
    goto fail;
  first_free_paddr = ROUNDUP(first_free_paddr, MEGAPAGE_SIZE);
  for (int i = 0; i < eh->e_phnum; i++)
    if (ph[i].p_type == PT_LOAD && ph[i].p_memsz && ph[i].p_vaddr < min_vaddr)
      min_vaddr = ph[i].p_vaddr;
  min_vaddr = ROUNDDOWN(min_vaddr, MEGAPAGE_SIZE);
  uintptr_t bias = first_free_paddr - min_vaddr;
  for (int i = eh->e_phnum - 1; i >= 0; i--) {
    if(ph[i].p_type == PT_LOAD && ph[i].p_memsz) {
      uintptr_t prepad = ph[i].p_vaddr % RISCV_PGSIZE;
      uintptr_t vaddr = ph[i].p_vaddr + bias;
      if (vaddr + ph[i].p_memsz > max_vaddr)
        max_vaddr = vaddr + ph[i].p_memsz;
      if (ph[i].p_offset + ph[i].p_filesz > size)
        goto fail;
      memcpy((void*)vaddr, blob + ph[i].p_offset, ph[i].p_filesz);
      memset((void*)vaddr - prepad, 0, prepad);
      memset((void*)vaddr + ph[i].p_filesz, 0, ph[i].p_memsz - ph[i].p_filesz);
    }
  }

  info->entry = eh->e_entry;
  info->load_offset = bias;
  info->first_user_vaddr = min_vaddr;
  info->first_vaddr_after_user = ROUNDUP(max_vaddr - bias, RISCV_PGSIZE);
  return;

fail:
    die("failed to load payload");
}
