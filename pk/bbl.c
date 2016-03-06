#include "boot.h"
#include "mtrap.h"
#include "vm.h"
#include "config.h"

static volatile int elf_loaded;

static void enter_entry_point()
{
  prepare_supervisor_mode();
  write_csr(mepc, current.entry);
  asm volatile("eret");
  __builtin_unreachable();
}

void run_loaded_program(size_t argc, char** argv)
{
  if (!current.is_supervisor)
    die("bbl can't run user binaries; try using pk instead");

  supervisor_vm_init();
#ifdef PK_ENABLE_LOGO
  print_logo();
#endif
  mb();
  elf_loaded = 1;
  enter_entry_point();
}

void boot_other_hart()
{
  while (!elf_loaded)
    ;
  mb();
  enter_entry_point();
}
