#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include <string.h>

static kernel_elf_info info;
static volatile int elf_loaded;

void boot_other_hart()
{
  while (!elf_loaded)
    ;
  mb();
  enter_supervisor_mode((void *)info.entry + info.load_offset, read_csr(mhartid), 0);
}

void boot_loader()
{
  extern char _payload_start, _payload_end;
  load_kernel_elf(&_payload_start, &_payload_end - &_payload_start, &info);
#ifdef PK_ENABLE_LOGO
  print_logo();
#endif
  mb();
  elf_loaded = 1;
  boot_other_hart();
}
