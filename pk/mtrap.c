#include "mtrap.h"
#include "frontend.h"
#include "mcall.h"
#include "vm.h"
#include <errno.h>

void illegal_insn_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  asm (".pushsection .rodata\n"
       "illegal_insn_trap_table:\n"
       "  .word truly_illegal_insn\n"
#ifdef PK_ENABLE_FP_EMULATION
       "  .word emulate_float_load\n"
#else
       "  .word truly_illegal_insn\n"
#endif
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
#ifdef PK_ENABLE_FP_EMULATION
       "  .word emulate_float_store\n"
#else
       "  .word truly_illegal_insn\n"
#endif
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_mul_div\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_mul_div32\n"
       "  .word truly_illegal_insn\n"
#ifdef PK_ENABLE_FP_EMULATION
       "  .word emulate_fmadd\n"
       "  .word emulate_fmadd\n"
       "  .word emulate_fmadd\n"
       "  .word emulate_fmadd\n"
       "  .word emulate_fp\n"
#else
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
#endif
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
#ifdef PK_ENABLE_FP_EMULATION
       "  .word emulate_system\n"
#else
       "  .word truly_illegal_insn\n"
#endif
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .popsection");

  uintptr_t mstatus;
  insn_t insn = get_insn(mepc, &mstatus);

  if ((insn & 3) != 3)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  write_csr(mepc, mepc + 4);

  extern int32_t illegal_insn_trap_table[];
  int32_t* pf = (void*)illegal_insn_trap_table + (insn & 0x7c);
  emulation_func f = (emulation_func)(uintptr_t)*pf;
  f(regs, mcause, mepc, mstatus, insn);
}

void __attribute__((noreturn)) bad_trap()
{
  panic("machine mode: unhandlable trap %d @ %p", read_csr(mcause), read_csr(mepc));
}

void htif_interrupt()
{
  uintptr_t fromhost = swap_csr(mfromhost, 0);
  if (!fromhost)
    return;

  uintptr_t dev = FROMHOST_DEV(fromhost);
  uintptr_t cmd = FROMHOST_CMD(fromhost);
  uintptr_t data = FROMHOST_DATA(fromhost);

  sbi_device_message* m = HLS()->device_request_queue_head;
  sbi_device_message* prev = NULL;
  for (size_t i = 0, n = HLS()->device_request_queue_size; i < n; i++) {
    if (!supervisor_paddr_valid(m, sizeof(*m))
        && EXTRACT_FIELD(read_csr(mstatus), MSTATUS_MPP) != PRV_M)
      panic("htif: page fault");

    sbi_device_message* next = (void*)m->sbi_private_data;
    if (m->dev == dev && m->cmd == cmd) {
      m->data = data;

      // dequeue from request queue
      if (prev)
        prev->sbi_private_data = (uintptr_t)next;
      else
        HLS()->device_request_queue_head = next;
      HLS()->device_request_queue_size = n-1;
      m->sbi_private_data = 0;

      // enqueue to response queue
      if (HLS()->device_response_queue_tail)
        HLS()->device_response_queue_tail->sbi_private_data = (uintptr_t)m;
      else
        HLS()->device_response_queue_head = m;
      HLS()->device_response_queue_tail = m;

      // signal software interrupt
      set_csr(mip, MIP_SSIP);
      return;
    }

    prev = m;
    m = (void*)atomic_read(&m->sbi_private_data);
  }

  panic("htif: no record");
}

static uintptr_t mcall_hart_id()
{
  return HLS()->hart_id;
}

static uintptr_t mcall_console_putchar(uint8_t ch)
{
  while (swap_csr(mtohost, TOHOST_CMD(1, 1, ch)) != 0);
  while (1) {
    uintptr_t fromhost = read_csr(mfromhost);
    if (FROMHOST_DEV(fromhost) != 1 || FROMHOST_CMD(fromhost) != 1) {
      if (fromhost)
        htif_interrupt();
      continue;
    }
    write_csr(mfromhost, 0);
    break;
  }
  return 0;
}

void printm(const char* s, ...)
{
  char buf[128];
  va_list vl;

  va_start(vl, s);
  vsnprintf(buf, sizeof buf, s, vl);
  va_end(vl);

  char* p = buf;
  while (*p)
    mcall_console_putchar(*p++);
}

static uintptr_t mcall_dev_req(sbi_device_message *m)
{
  //printm("req %d %p\n", HLS()->device_request_queue_size, m);
  if (!supervisor_paddr_valid(m, sizeof(*m))
      && EXTRACT_FIELD(read_csr(mstatus), MSTATUS_MPP) != PRV_M)
    return -EFAULT;

  if ((m->dev > 0xFFU) | (m->cmd > 0xFFU) | (m->data > 0x0000FFFFFFFFFFFFU))
    return -EINVAL;

  while (swap_csr(mtohost, TOHOST_CMD(m->dev, m->cmd, m->data)) != 0)
    ;

  m->sbi_private_data = (uintptr_t)HLS()->device_request_queue_head;
  HLS()->device_request_queue_head = m;
  HLS()->device_request_queue_size++;

  return 0;
}

static uintptr_t mcall_dev_resp()
{
  htif_interrupt();

  sbi_device_message* m = HLS()->device_response_queue_head;
  if (m) {
    //printm("resp %p\n", m);
    sbi_device_message* next = (void*)atomic_read(&m->sbi_private_data);
    HLS()->device_response_queue_head = next;
    if (!next) {
      HLS()->device_response_queue_tail = 0;

      // only clear SSIP if no other events are pending
      clear_csr(mip, MIP_SSIP);
      mb();
      if (HLS()->ipi_pending & IPI_SOFT)
        set_csr(mip, MIP_SSIP);
    }
  }
  return (uintptr_t)m;
}

static void send_ipi(uintptr_t recipient, int event)
{
  if ((atomic_or(&OTHER_HLS(recipient)->ipi_pending, event) & event) == 0) {
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

static uintptr_t mcall_clear_ipi()
{
  // only clear SSIP if no other events are pending
  if (HLS()->device_response_queue_head == NULL) {
    clear_csr(mip, MIP_SSIP);
    mb();
  }

  return atomic_swap(&HLS()->ipi_pending, 0);
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
  int ipi_pending = atomic_swap(&HLS()->ipi_pending, 0);

  if (ipi_pending & IPI_SOFT)
    set_csr(mip, MIP_SSIP);

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
      while (OTHER_HLS(i)->ipi_pending & event)
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
    case MCALL_SEND_DEVICE_REQUEST:
      retval = mcall_dev_req((sbi_device_message*)arg0);
      break;
    case MCALL_RECEIVE_DEVICE_RESPONSE:
      retval = mcall_dev_resp();
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
