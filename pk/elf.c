// See LICENSE for license details.

#include "pk.h"
#include "mtrap.h"
#include "boot.h"
#include "bits.h"
#include "elf.h"
#include "usermem.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "mmap.h"

/**
 * The protection flags are in the p_flags section of the program header.
 * But rather annoyingly, they are the reverse of what mmap expects.
 */
static inline int get_prot(uint32_t p_flags)
{
  int prot_x = (p_flags & PF_X) ? PROT_EXEC  : PROT_NONE;
  int prot_w = (p_flags & PF_W) ? PROT_WRITE : PROT_NONE;
  int prot_r = (p_flags & PF_R) ? PROT_READ  : PROT_NONE;

  return (prot_x | prot_w | prot_r);
}

int allocate_shadow_stack(unsigned long *shstk_base, unsigned long *shstk_size) {
  int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
  size_t mem_pages = mem_size >> RISCV_PGSHIFT;
  size_t stack_size = MIN(mem_pages >> 5, 2048) * RISCV_PGSIZE;
  uintptr_t addr = __do_mmap(current.mmap_max - stack_size - 0x1000, 0x1000, PROT_WRITE, flags, 0, 0);
  /*printf("pk assign %lx\n", addr);*/
  addr += 0x1000;
  asm volatile("csrw 0x11, %0\n":: "r"(addr));
  *shstk_base = addr;
  return 0;
}

static int riscv_parse_elf_property(uint32_t type, const void *data,
                                    size_t datasz) {
  if (type == GUN_PROPERTY_RISCV_FEATURE_1_AND) {
    const uint32_t *pr = data;
    if (datasz != sizeof(*pr))
      return -1;

    if (*pr & GNU_PROPERTY_RISCV_FEATURE_1_ZICFILP)
      current.lpad = true;

    if (*pr & GNU_PROPERTY_RISCV_FEATURE_1_ZICFISS)
      current.shstk = true;
  }

  return 0;
}

static int parse_elf_property(const char *data, size_t *off, size_t datasz) {
  size_t step, offset;
  const struct gnu_property *pr;
  int ret;

  if (*off == datasz)
    return -1;

  offset = *off;
  datasz -= offset;

  if (datasz < sizeof(*pr))
    return -1;

  pr = (const struct gnu_property *) (data + offset);
  offset += sizeof(*pr);
  datasz -= sizeof(*pr);

  if (pr->pr_datasz > datasz)
    return -1;

  step = pr->pr_datasz;
  if (step > pr->pr_datasz)
    return -1;

  ret = riscv_parse_elf_property(pr->pr_type, data + offset, pr->pr_datasz);

  if (ret)
    return ret;

  *off = offset + step;

  return 0;
}

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

#if __riscv_xlen == 64
  assert(IS_ELF64(eh));
#else
  assert(IS_ELF32(eh));
#endif

#ifndef __riscv_compressed
  assert(!(eh.e_flags & EF_RISCV_RVC));
#endif

  size_t phdr_size = eh.e_phnum * sizeof(Elf_Phdr);
  if (phdr_size > info->phdr_size)
    goto fail;
  ssize_t ret = file_pread(file, (void*)info->phdr, phdr_size, eh.e_phoff);
  if (ret < (ssize_t)phdr_size)
    goto fail;
  info->phnum = eh.e_phnum;
  info->phent = sizeof(Elf_Phdr);
  Elf_Phdr* ph = (typeof(ph))info->phdr;

  // compute highest VA in ELF
  uintptr_t max_vaddr = 0;
  for (int i = 0; i < eh.e_phnum; i++)
    if (ph[i].p_type == PT_LOAD && ph[i].p_memsz)
      max_vaddr = MAX(max_vaddr, ph[i].p_vaddr + ph[i].p_memsz);
  max_vaddr = ROUNDUP(max_vaddr, RISCV_PGSIZE);

  // don't load dynamic linker at 0, else we can't catch NULL pointer derefs
  uintptr_t bias = 0;
  if (eh.e_type == ET_DYN)
    bias = RISCV_PGSIZE;

  info->entry = eh.e_entry + bias;
  int flags = MAP_FIXED | MAP_PRIVATE;
  for (int i = eh.e_phnum - 1; i >= 0; i--) {

    if (ph[i].p_type == PT_GNU_PROPERTY) {
      union {
        Elf_Note nhdr;
        char data[0x400];
      } note;
      file_pread(file, &note, sizeof(note), ph[i].p_offset);
      size_t off = sizeof(note.nhdr) + NOTE_NAME_SZ;
      int ret;
      size_t datasz = off + note.nhdr.n_descsz;
      if (note.nhdr.n_type == NT_GNU_PROPERTY_TYPE_0) {
        do {
          ret = parse_elf_property(note.data, &off, datasz);
        } while (!ret);
      }
    }

    if(ph[i].p_type == PT_INTERP) {
      panic("not a statically linked ELF program");
    }
    if(ph[i].p_type == PT_LOAD && ph[i].p_memsz) {
      uintptr_t prepad = ph[i].p_vaddr % RISCV_PGSIZE;
      uintptr_t vaddr = ph[i].p_vaddr + bias;
      if (vaddr + ph[i].p_memsz > info->brk_min)
        info->brk_min = vaddr + ph[i].p_memsz;
      int flags2 = flags | (prepad ? MAP_POPULATE : 0);
      int prot = get_prot(ph[i].p_flags);
      if (__do_mmap(vaddr - prepad, ph[i].p_filesz + prepad, prot | PROT_WRITE, flags2, file, ph[i].p_offset - prepad) != vaddr - prepad)
        goto fail;
      memset_user((void*)vaddr - prepad, 0, prepad);
      if (!(prot & PROT_WRITE))
        if (do_mprotect(vaddr - prepad, ph[i].p_filesz + prepad, prot))
          goto fail;
      size_t mapped = ROUNDUP(ph[i].p_filesz + prepad, RISCV_PGSIZE) - prepad;
      if (ph[i].p_memsz > mapped)
        if (__do_mmap(vaddr + mapped, ph[i].p_memsz - mapped, prot, flags|MAP_ANONYMOUS, 0, 0) != vaddr + mapped)
          goto fail;
    }
  }

  unsigned long shstk_base = 0;
  unsigned long shstk_size = 0;
  if (current.shstk)
    allocate_shadow_stack(&shstk_base, &shstk_size);

  file_decref(file);
  info->brk = ROUNDUP(info->brk_min, RISCV_PGSIZE);
  return;

fail:
  panic("couldn't open ELF program: %s!", fn);
}
