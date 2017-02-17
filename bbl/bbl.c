#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include <string.h>

static volatile uintptr_t entry_point;

void boot_other_hart()
{
  while (!entry_point)
    ;
  mb();
  enter_supervisor_mode((void *)entry_point, read_csr(mhartid), 0);
}

void boot_loader()
{
  extern char _payload_start, _payload_end;
  uintptr_t entry = load_kernel_elf(&_payload_start, &_payload_end - &_payload_start);
#ifdef PK_ENABLE_LOGO
  print_logo();
#endif
  mb();
  entry_point = entry;
  boot_other_hart();
}
