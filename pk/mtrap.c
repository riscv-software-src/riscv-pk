#include "mtrap.h"
#include "frontend.h"
#include "mcall.h"
#include "vm.h"
#include <errno.h>

void __attribute__((noreturn)) bad_trap()
{
  panic("machine mode: unhandlable trap %d @ %p", read_csr(mcause), read_csr(mepc));
}

static uintptr_t mcall_hart_id()
{
  return read_const_csr(mhartid);
}

static void request_htif_keyboard_interrupt()
{
  uintptr_t old_tohost = swap_csr(mtohost, TOHOST_CMD(1, 0, 0));
  kassert(old_tohost == 0);
}

void htif_interrupt()
{
  // we should only be interrupted by keypresses
  uintptr_t fromhost = swap_csr(mfromhost, 0);
  kassert(FROMHOST_DEV(fromhost) == 1 && FROMHOST_CMD(fromhost) == 0);
  HLS()->console_ibuf = 1 + (uint8_t)FROMHOST_DATA(fromhost);
  set_csr(mip, MIP_SSIP);
}

static void do_tohost_fromhost(uintptr_t dev, uintptr_t cmd, uintptr_t data)
{
  while (swap_csr(mtohost, TOHOST_CMD(dev, cmd, data)) != 0)
    if (read_csr(mfromhost))
      htif_interrupt();

  while (1) {
    uintptr_t fromhost = read_csr(mfromhost);
    if (fromhost) {
      if (FROMHOST_DEV(fromhost) == dev && FROMHOST_CMD(fromhost) == cmd) {
        write_csr(mfromhost, 0);
        break;
      }
      htif_interrupt();
    }
  }
}

static uintptr_t mcall_console_putchar(uint8_t ch)
{
  do_tohost_fromhost(1, 1, ch);
  return 0;
}

static uintptr_t mcall_htif_syscall(uintptr_t magic_mem)
{
  do_tohost_fromhost(0, 0, magic_mem);
  return 0;
}

void printm(const char* s, ...)
{
  char buf[256];
  va_list vl;

  va_start(vl, s);
  vsnprintf(buf, sizeof buf, s, vl);
  va_end(vl);

  for (const char* p = buf; *p; p++)
    mcall_console_putchar(*p);
}

static void send_ipi(uintptr_t recipient, int event)
{
  if ((atomic_or(&OTHER_HLS(recipient)->mipi_pending, event) & event) == 0) {
    mb();
    OTHER_HLS(recipient)->csrs[CSR_MIPI] = 1;
  }
}

static uintptr_t mcall_send_ipi(uintptr_t recipient)
{
  if (recipient >= num_harts)
    return -1;

  send_ipi(recipient, IPI_SOFT);
  return 0;
}

static void reset_ssip()
{
  clear_csr(mip, MIP_SSIP);
  mb();

  if (HLS()->sipi_pending || HLS()->console_ibuf > 0)
    set_csr(mip, MIP_SSIP);
}

static uintptr_t mcall_console_getchar()
{
  int ch = atomic_swap(&HLS()->console_ibuf, -1);
  if (ch >= 0)
    request_htif_keyboard_interrupt();
  reset_ssip();
  return ch - 1;
}

static uintptr_t mcall_clear_ipi()
{
  int ipi = atomic_swap(&HLS()->sipi_pending, 0);
  reset_ssip();
  return ipi;
}

static uintptr_t mcall_shutdown()
{
  while (1)
    write_csr(mtohost, 1);
  return 0;
}

static uintptr_t mcall_set_timer(unsigned long long when)
{
  // bbl/pk don't use the timer, so there's no need to virtualize it
  write_csr(mtimecmp, when);
#ifndef __riscv64
  write_csr(mtimecmph, when >> 32);
#endif
  clear_csr(mip, MIP_STIP);
  set_csr(mie, MIP_MTIP);
  return 0;
}

void software_interrupt()
{
  clear_csr(mip, MIP_MSIP);
  mb();
  int ipi_pending = atomic_swap(&HLS()->mipi_pending, 0);

  if (ipi_pending & IPI_SOFT) {
    HLS()->sipi_pending = 1;
    set_csr(mip, MIP_SSIP);
  }

  if (ipi_pending & IPI_FENCE_I)
    asm volatile ("fence.i");

  if (ipi_pending & IPI_SFENCE_VM)
    asm volatile ("sfence.vm");
}

static void send_ipi_many(uintptr_t* pmask, int event)
{
  kassert(MAX_HARTS <= 8 * sizeof(*pmask));
  uintptr_t mask = -1;
  if (pmask) {
    kassert(supervisor_paddr_valid((uintptr_t)pmask, sizeof(uintptr_t)));
    mask = *pmask;
  }

  // send IPIs to everyone
  for (ssize_t i = num_harts-1; i >= 0; i--)
    if ((mask >> i) & 1)
      send_ipi(i, event);

  // wait until all events have been handled.
  // prevent deadlock while spinning by handling any IPIs from other harts.
  for (ssize_t i = num_harts-1; i >= 0; i--)
    if ((mask >> i) & 1)
      while (OTHER_HLS(i)->mipi_pending & event)
        software_interrupt();
}

static uintptr_t mcall_remote_sfence_vm(uintptr_t* hart_mask, uintptr_t asid)
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
  switch (n)
  {
    case MCALL_HART_ID:
      retval = mcall_hart_id();
      break;
    case MCALL_CONSOLE_PUTCHAR:
      retval = mcall_console_putchar(arg0);
      break;
    case MCALL_CONSOLE_GETCHAR:
      retval = mcall_console_getchar();
      break;
    case MCALL_HTIF_SYSCALL:
      retval = mcall_htif_syscall(arg0);
      break;
    case MCALL_SEND_IPI:
      retval = mcall_send_ipi(arg0);
      break;
    case MCALL_CLEAR_IPI:
      retval = mcall_clear_ipi();
      break;
    case MCALL_SHUTDOWN:
      retval = mcall_shutdown();
      break;
    case MCALL_SET_TIMER:
      retval = mcall_set_timer(arg0);
      break;
    case MCALL_REMOTE_SFENCE_VM:
      retval = mcall_remote_sfence_vm((uintptr_t*)arg0, arg1);
      break;
    case MCALL_REMOTE_FENCE_I:
      retval = mcall_remote_fence_i((uintptr_t*)arg0);
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

  uintptr_t prev_priv = EXTRACT_FIELD(mstatus, MSTATUS_MPP);
  uintptr_t prev_ie = EXTRACT_FIELD(mstatus, MSTATUS_MPIE);
  kassert(prev_priv <= PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_SPP, prev_priv);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_SPIE, prev_ie);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPP, PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPIE, 0);
  write_csr(mstatus, mstatus);

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
    case CAUSE_MACHINE_ECALL:
      return mcall_trap(regs, mcause, mepc);
    default:
      bad_trap();
  }
}
