#include "pcr.h"
#include "pk.h"

void __attribute__((section(".boottext"))) __start()
{
  extern char stack_top;
  asm volatile("move $sp,%0" : : "r"(&stack_top-64) : "memory");

  extern char trap_entry;
  register void* te = &trap_entry;
  mtpcr(te,PCR_EVEC);

  mtpcr(0,PCR_COUNT);
  mtpcr(0,PCR_COMPARE);

  register long sr0 = SR_S | SR_PS | SR_ET | SR_IM | SR_EC;
  if(sizeof(void*) == 8)
    sr0 |= SR_SX;

  mtpcr(sr0 | SR_EF, PCR_SR);
  have_fp = mfpcr(PCR_SR) & SR_EF;
  mtpcr(sr0, PCR_SR);

  extern void boot();
  register void (*boot_p)() = &boot;
  asm("" : "=r"(boot_p) : "0"(boot_p));
  boot_p();
}
