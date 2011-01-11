#include "pcr.h"
#include "pk.h"

int have_fp = 1;

void handle_breakpoint(trapframe_t* tf)
{
  printk("Breakpoint\n");
  dump_tf(tf);
  tf->epc += 4;
}

void handle_fp_disabled(trapframe_t* tf)
{
  if(have_fp)
  {
    init_fp_regs();
    tf->sr |= SR_EF;
  }
  else
  {
#ifdef PK_ENABLE_FP_EMULATION
    if(emulate_fp(tf) != 0)
    {
      dump_tf(tf);
      panic("FPU emulation failed!");
    }
#else
    panic("FPU emulation disabled! pc %lx, insn %x",tf->epc,(uint32_t)tf->insn);
#endif
  }
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
#ifdef PK_ENABLE_FP_EMULATION
  if(emulate_fp(tf) == 0)
    return;
#endif

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

void handle_fault_load(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Faulting load!");
}

void handle_fault_store(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Faulting store!");
}

void handle_timer_interrupt(trapframe_t* tf)
{
  mtpcr(mfpcr(PCR_COMPARE)+TIMER_PERIOD,PCR_COMPARE);
}

void handle_trap(trapframe_t* tf)
{
  typedef void (*trap_handler)(trapframe_t*);
  const static trap_handler trap_handlers[] = {
    handle_misaligned_fetch,
    handle_fault_fetch,
    handle_illegal_instruction,
    handle_privileged_instruction,
    handle_fp_disabled,
    handle_syscall,
    handle_breakpoint,
    handle_misaligned_ldst,
    handle_fault_load,
    handle_fault_store,
    handle_badtrap,
    handle_badtrap,
    handle_badtrap,
    handle_badtrap,
    handle_badtrap,
    handle_badtrap,
    handle_badtrap, /* irq 0 */
    handle_badtrap, /* irq 1 */
    handle_badtrap, /* irq 2 */
    handle_badtrap, /* irq 3 */
    handle_badtrap, /* irq 4 */
    handle_badtrap, /* irq 5 */
    handle_badtrap, /* irq 6 */
    handle_timer_interrupt, /* irq 7 */
  };

  kassert(tf->cause < sizeof(trap_handlers)/sizeof(trap_handlers[0]));

  trap_handlers[tf->cause](tf);

  pop_tf(tf);
}
