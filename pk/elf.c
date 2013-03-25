// See LICENSE for license details.

#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>
#include <string.h>
#include "file.h"
#include "pk.h"

long load_elf(const char* fn, int* user64)
{
  sysret_t ret = file_open(fn, strlen(fn)+1, O_RDONLY, 0);
  file_t* file = (file_t*)ret.result;
  if(ret.result == -1)
    goto fail;

  char buf[2048]; // XXX
  int header_size = file_read(file, buf, sizeof(buf)).result;
  const Elf64_Ehdr* eh64 = (const Elf64_Ehdr*)buf;
  if(header_size < (int)sizeof(Elf64_Ehdr) ||
     !(eh64->e_ident[0] == '\177' && eh64->e_ident[1] == 'E' &&
       eh64->e_ident[2] == 'L'    && eh64->e_ident[3] == 'F'))
    goto fail;

  #define LOAD_ELF do { \
    eh = (typeof(eh))buf; \
    kassert(header_size >= eh->e_phoff + eh->e_phnum*sizeof(*ph)); \
    ph = (typeof(ph))(buf+eh->e_phoff); \
    for(int i = 0; i < eh->e_phnum; i++, ph++) { \
      if(ph->p_type == SHT_PROGBITS && ph->p_memsz) { \
        extern char _end; \
        if((char*)(long)ph->p_vaddr < &_end) \
        { \
          long diff = &_end - (char*)(long)ph->p_vaddr; \
          ph->p_vaddr += diff; \
          ph->p_offset += diff; \
          ph->p_memsz = diff >= ph->p_memsz ? 0 : ph->p_memsz - diff; \
          ph->p_filesz = diff >= ph->p_filesz ? 0 : ph->p_filesz - diff; \
        } \
        if(file_pread(file, (char*)(long)ph->p_vaddr, ph->p_filesz, ph->p_offset).result != ph->p_filesz) \
          goto fail; \
        memset((char*)(long)ph->p_vaddr+ph->p_filesz, 0, ph->p_memsz-ph->p_filesz); \
      } \
    } \
  } while(0)

  long entry;
  *user64 = 0;
  if (IS_ELF32(*eh64))
  {
    Elf32_Ehdr* eh;
    Elf32_Phdr* ph;
    LOAD_ELF;
    entry = eh->e_entry;
  }
  else if (IS_ELF64(*eh64))
  {
    *user64 = 1;
    Elf64_Ehdr* eh;
    Elf64_Phdr* ph;
    LOAD_ELF;
    entry = eh->e_entry;
  }
  else
    goto fail;

  file_decref(file);

  return entry;

fail:
    panic("couldn't open ELF program: %s!", fn);
}
