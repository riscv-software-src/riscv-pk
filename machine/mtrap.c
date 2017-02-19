#include "mtrap.h"
#include "mcall.h"
#include "htif.h"
#include "atomic.h"
#include "bits.h"
#include "vm.h"
#include "unprivileged_memory.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

void __attribute__((noreturn)) bad_trap()
{
  die("machine mode: unhandlable trap %d @ %p", read_csr(mcause), read_csr(mepc));
}

static uintptr_t mcall_console_putchar(uint8_t ch)
{
  htif_console_putchar(ch);
  return 0;
}

void poweroff()
{
  htif_poweroff();
}

void putstring(const char* s)
{
  while (*s)
    mcall_console_putchar(*s++);
}

void printm(const char* s, ...)
{
  char buf[256];
  va_list vl;

  va_start(vl, s);
  vsnprintf(buf, sizeof buf, s, vl);
  va_end(vl);

  putstring(buf);
}

static void send_ipi(uintptr_t recipient, int event)
{
  if ((atomic_or(&OTHER_HLS(recipient)->mipi_pending, event) & event) == 0) {
    mb();
    *OTHER_HLS(recipient)->ipi = 1;
  }
}

static uintptr_t mcall_console_getchar()
{
  return htif_console_getchar();
}

static uintptr_t mcall_clear_ipi()
{
  return clear_csr(mip, MIP_SSIP) & MIP_SSIP;
}

static uintptr_t mcall_shutdown()
{
  poweroff();
}

static uintptr_t mcall_set_timer(uint64_t when)
{
  *HLS()->timecmp = when;
  clear_csr(mip, MIP_STIP);
  set_csr(mie, MIP_MTIP);
  return 0;
}

static void send_ipi_many(uintptr_t* pmask, int event)
{
  _Static_assert(MAX_HARTS <= 8 * sizeof(*pmask), "# harts > uintptr_t bits");
  uintptr_t mask = -1;
  if (pmask)
    mask = load_uintptr_t(pmask, read_csr(mepc));

  // send IPIs to everyone
  for (ssize_t i = num_harts-1; i >= 0; i--)
    if ((mask >> i) & 1)
      send_ipi(i, event);

  // wait until all events have been handled.
  // prevent deadlock by consuming incoming IPIs.
  uint32_t incoming_ipi = 0;
  for (ssize_t i = num_harts-1; i >= 0; i--)
    if ((mask >> i) & 1)
      while (OTHER_HLS(i)->ipi)
        incoming_ipi |= atomic_swap(HLS()->ipi, 0);

  // if we got an IPI, restore it; it will be taken after returning
  if (incoming_ipi) {
    *HLS()->ipi = incoming_ipi;
    mb();
  }
}

static uintptr_t mcall_remote_sfence_vm(uintptr_t* hart_mask)
{
  // ignore the ASID and do a global flush.
  // this allows us to avoid queueing a message.
  send_ipi_many(hart_mask, IPI_SFENCE_VM);
  return 0;
}

static uintptr_t mcall_remote_fence_i(uintptr_t* hart_mask)
{
  send_ipi_many(hart_mask, IPI_FENCE_I);
  return 0;
}

void mcall_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  uintptr_t n = regs[17], arg0 = regs[10], arg1 = regs[11], retval;
  uintptr_t ipi_type = 0;

  switch (n)
  {
    case MCALL_CONSOLE_PUTCHAR:
      retval = mcall_console_putchar(arg0);
      break;
    case MCALL_CONSOLE_GETCHAR:
      retval = mcall_console_getchar();
      break;
    case MCALL_SEND_IPI:
      ipi_type = IPI_SOFT;
    case MCALL_REMOTE_SFENCE_VM:
      ipi_type = IPI_SFENCE_VM;
    case MCALL_REMOTE_FENCE_I:
      ipi_type = IPI_FENCE_I;
      send_ipi_many((uintptr_t*)arg0, ipi_type);
      retval = 0;
      break;
    case MCALL_CLEAR_IPI:
      retval = mcall_clear_ipi();
      break;
    case MCALL_SHUTDOWN:
      retval = mcall_shutdown();
      break;
    case MCALL_SET_TIMER:
#if __riscv_xlen == 32
      retval = mcall_set_timer(arg0 + ((uint64_t)arg1 << 32));
#else
      retval = mcall_set_timer(arg0);
#endif
      break;
    default:
      retval = -ENOSYS;
      break;
  }
  regs[10] = retval;
  write_csr(mepc, mepc + 4);
}

void redirect_trap(uintptr_t epc, uintptr_t mstatus)
{
  write_csr(sepc, epc);
  write_csr(scause, read_csr(mcause));
  write_csr(mepc, read_csr(stvec));

  uintptr_t new_mstatus = mstatus & ~(MSTATUS_SPP | MSTATUS_SPIE | MSTATUS_MPIE);
  uintptr_t mpp_s = MSTATUS_MPP & (MSTATUS_MPP >> 1);
  new_mstatus |= (mstatus / (MSTATUS_MPIE / MSTATUS_SPIE)) & MSTATUS_SPIE;
  new_mstatus |= (mstatus / (mpp_s / MSTATUS_SPP)) & MSTATUS_SPP;
  new_mstatus |= mpp_s;
  write_csr(mstatus, new_mstatus);

  extern void __redirect_trap();
  return __redirect_trap();
}

static void machine_page_fault(uintptr_t* regs, uintptr_t mepc)
{
  // MPRV=1 iff this trap occurred while emulating an instruction on behalf
  // of a lower privilege level. In that case, a2=epc and a3=mstatus.
  if (read_csr(mstatus) & MSTATUS_MPRV) {
    write_csr(sbadaddr, read_csr(mbadaddr));
    return redirect_trap(regs[12], regs[13]);
  }
  bad_trap();
}

void trap_from_machine_mode(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  uintptr_t mcause = read_csr(mcause);

  switch (mcause)
  {
    case CAUSE_FAULT_LOAD:
    case CAUSE_FAULT_STORE:
      return machine_page_fault(regs, mepc);
    default:
      bad_trap();
  }
}
