#include "pcr.h"

void __attribute__((section(".boottext"))) __start()
{
  extern char stack_top;
  asm("move $sp,%0" : : "r"(&stack_top-64));

  register long sr0 = SR_S | SR_PS | SR_ET | SR_UX | SR_KX;
  mtpcr(sr0,PCR_SR);

  extern char trap_table;
  register void* tt = &trap_table;
  mtpcr(tt,PCR_TBR);

  extern void boot();
  register void (*boot_p)() = &boot;
  asm("" : "=r"(boot_p) : "0"(boot_p));
  boot_p();
}
