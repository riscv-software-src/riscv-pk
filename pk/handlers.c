#include "pcr.h"
#include "pk.h"

int have_fp = 1; // initialized to 1 because it can't be in the .bss section!
int have_vector = 1;

static void handle_fp_disabled(trapframe_t* tf)
{
  if(have_fp)
    init_fp(tf);
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

static void handle_vector_disabled(trapframe_t* tf)
{
  if (have_vector)
    tf->sr |= SR_EV;
  else
    panic("No vector hardware! pc %lx, insn %x",tf->epc,(uint32_t)tf->insn);
}

static void handle_privileged_instruction(trapframe_t* tf)
{
  dump_tf(tf);
  panic("A privileged instruction was executed!");
}

static void handle_illegal_instruction(trapframe_t* tf)
{
#ifdef PK_ENABLE_FP_EMULATION
  if(emulate_fp(tf) == 0)
    return;
#endif

  dump_tf(tf);
  panic("An illegal instruction was executed!");
}

static void handle_breakpoint(trapframe_t* tf)
{
  dump_tf(tf);
  printk("Breakpoint!\n");
}

static void handle_misaligned_fetch(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Misaligned instruction access!");
}

void handle_misaligned_ldst(trapframe_t* tf)
{
  dump_tf(tf);
  panic("Misaligned data access!");
}

static void handle_fault_fetch(trapframe_t* tf)
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

static void handle_bad_interrupt(trapframe_t* tf, int interrupt)
{
  panic("Bad interrupt %d!",interrupt);
}

static void handle_timer_interrupt(trapframe_t* tf)
{
  mtpcr(mfpcr(PCR_COMPARE)+TIMER_PERIOD,PCR_COMPARE);
}

static void handle_interrupt(trapframe_t* tf)
{
  int interrupts = (tf->cause & CAUSE_IP) >> CAUSE_IP_SHIFT;

  for(int i = 0; interrupts; interrupts >>= 1, i++)
  {
    if(i == TIMER_IRQ)
      handle_timer_interrupt(tf);
    else
      handle_bad_interrupt(tf,i);
  }
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
    handle_interrupt,
    handle_syscall,
    handle_breakpoint,
    handle_misaligned_ldst,
    handle_fault_load,
    handle_fault_store,
    handle_vector_disabled,
  };

  int exccode = (tf->cause & CAUSE_EXCCODE) >> CAUSE_EXCCODE_SHIFT;
  kassert(exccode < sizeof(trap_handlers)/sizeof(trap_handlers[0]));

  trap_handlers[exccode](tf);

  pop_tf(tf);
}
