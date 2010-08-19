#include "pcr.h"
#include "pk.h"

void handle_breakpoint(trapframe_t* tf)
{
  printk("Breakpoint\n");
  dump_tf(tf);
  tf->epc += 4;
  pop_tf(tf);
}

void handle_fp_disabled(trapframe_t* tf)
{
  tf->sr |= SR_EF;
  pop_tf(tf);
}

void handle_badtrap(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Bad trap vector!");
}

void handle_privileged_instruction(trapframe_t* tf)
{
  dump_tf(tf);
  panic("A privileged instruction was executed!");
}

void handle_illegal_instruction(trapframe_t* tf)
{
  dump_tf(tf);
  panic("An illegal instruction was executed!");
}

void handle_misaligned_fetch(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Misaligned instruction access!");
}

void handle_misaligned_ldst(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Misaligned data access!");
}

void handle_fault_fetch(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Faulting instruction access!");
}

void handle_fault_ldst(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Faulting data access!");
}
