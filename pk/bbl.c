#include "pk.h"
#include "vm.h"

void run_loaded_program(struct mainvars* args)
{
  if (!current.is_supervisor)
    panic("bbl can't run user binaries; try using pk instead");

    supervisor_vm_init();
#ifdef PK_ENABLE_LOGO
    print_logo();
#endif
    write_csr(mepc, current.entry);
    asm volatile("eret");
    __builtin_unreachable();
}
