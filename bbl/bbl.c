#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include <string.h>

static const void* entry_point;

void boot_other_hart()
{
  const void* entry;
  do {
    entry = entry_point;
    mb();
  } while (!entry);
  enter_supervisor_mode(entry, read_csr(mhartid), 0);
}

void boot_loader()
{
  extern char _payload_start;
#ifdef PK_ENABLE_LOGO
  print_logo();
#endif
  mb();
  entry_point = &_payload_start;
  boot_other_hart();
}
