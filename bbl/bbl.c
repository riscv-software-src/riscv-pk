#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include <string.h>

static const void* entry_point;

void boot_other_hart(uintptr_t dtb)
{
  const void* entry;
  do {
    entry = entry_point;
    mb();
  } while (!entry);
  enter_supervisor_mode(entry, read_csr(mhartid), dtb);
}

void boot_loader(uintptr_t dtb)
{
  extern char _payload_start;
#ifdef PK_ENABLE_LOGO
  print_logo();
#endif
  mb();
  entry_point = &_payload_start;
  boot_other_hart(dtb);
}
