#include "mtrap.h"
#include "frontend.h"
#include "hcall.h"
#include "vm.h"
#include <errno.h>

uintptr_t illegal_insn_trap(uintptr_t mcause, uintptr_t* regs)
{
  asm (".pushsection .rodata\n"
       "illegal_insn_trap_table:\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_float_load\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_float_store\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_mul_div\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_mul_div32\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fmadd\n"
       "  .word emulate_fmsub\n"
       "  .word emulate_fnmsub\n"
       "  .word emulate_fnmadd\n"
       "  .word emulate_fp\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_system\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .popsection");

  uintptr_t mstatus = read_csr(mstatus);
  uintptr_t mepc = read_csr(mepc);

  insn_fetch_t fetch = get_insn(mcause, mstatus, mepc);

  if (fetch.error || (fetch.insn & 3) != 3)
    return -1;

  extern int32_t illegal_insn_trap_table[];
  int32_t* pf = (void*)illegal_insn_trap_table + (fetch.insn & 0x7c);
  emulation_func f = (emulation_func)(uintptr_t)*pf;
  return f(mcause, regs, fetch.insn, mstatus, mepc);
}

void __attribute__((noreturn)) bad_trap()
{
  panic("machine mode: unhandlable trap %d @ %p", read_csr(mcause), read_csr(mepc));
}

uintptr_t htif_interrupt(uintptr_t mcause, uintptr_t* regs)
{
  uintptr_t fromhost = swap_csr(fromhost, 0);
  if (!fromhost)
    return 0;

  uintptr_t dev = FROMHOST_DEV(fromhost);
  uintptr_t cmd = FROMHOST_CMD(fromhost);
  uintptr_t data = FROMHOST_DATA(fromhost);

  sbi_device_message* m = MAILBOX()->device_request_queue_head;
  sbi_device_message* prev = NULL;
  for (size_t i = 0, n = MAILBOX()->device_request_queue_size; i < n; i++) {
    if (!supervisor_paddr_valid(m, sizeof(*m))
        && EXTRACT_FIELD(read_csr(mstatus), MSTATUS_PRV1) != PRV_M)
      panic("htif: page fault");

    sbi_device_message* next = (void*)m->sbi_private_data;
    if (m->dev == dev && m->cmd == cmd) {
      m->data = data;

      // dequeue from request queue
      if (prev)
        prev->sbi_private_data = (uintptr_t)next;
      else
        MAILBOX()->device_request_queue_head = next;
      MAILBOX()->device_request_queue_size = n-1;
      m->sbi_private_data = 0;

      // enqueue to response queue
      if (MAILBOX()->device_response_queue_tail)
        MAILBOX()->device_response_queue_tail->sbi_private_data = (uintptr_t)m;
      else
        MAILBOX()->device_response_queue_head = m;
      MAILBOX()->device_response_queue_tail = m;

      // signal software interrupt
      set_csr(mstatus, MSTATUS_SSIP);
      return 0;
    }

    prev = m;
    m = (void*)atomic_read(&m->sbi_private_data);
  }

  panic("htif: no record");
}

static uintptr_t hcall_console_putchar(uint8_t ch)
{
  while (swap_csr(tohost, TOHOST_CMD(1, 1, ch)) != 0);
  while (1) {
    uintptr_t fromhost = read_csr(fromhost);
    if (FROMHOST_DEV(fromhost) != 1 || FROMHOST_CMD(fromhost) != 1) {
      if (fromhost)
        htif_interrupt(0, 0);
      continue;
    }
    write_csr(fromhost, 0);
    break;
  }
  return 0;
}

#define printm(str, ...) ({ \
  char buf[1024], *p = buf; sprintk(buf, str, __VA_ARGS__); \
  while (*p) hcall_console_putchar(*p++); })

static uintptr_t hcall_dev_req(sbi_device_message *m)
{
  //printm("req %d %p\n", MAILBOX()->device_request_queue_size, m);
#ifndef __riscv64
  return -ENOSYS; // TODO: RV32 HTIF?
#else
  if (!supervisor_paddr_valid(m, sizeof(*m))
      && EXTRACT_FIELD(read_csr(mstatus), MSTATUS_PRV1) != PRV_M)
    return -EFAULT;

  if ((m->dev > 0xFFU) | (m->cmd > 0xFFU) | (m->data > 0x0000FFFFFFFFFFFFU))
    return -EINVAL;

  while (swap_csr(tohost, TOHOST_CMD(m->dev, m->cmd, m->data)) != 0)
    ;

  m->sbi_private_data = (uintptr_t)MAILBOX()->device_request_queue_head;
  MAILBOX()->device_request_queue_head = m;
  MAILBOX()->device_request_queue_size++;

  return 0;
#endif
}

static uintptr_t hcall_dev_resp()
{
  htif_interrupt(0, 0);

  sbi_device_message* m = MAILBOX()->device_response_queue_head;
  if (m) {
    //printm("resp %p\n", m);
    sbi_device_message* next = (void*)atomic_read(&m->sbi_private_data);
    MAILBOX()->device_response_queue_head = next;
    if (!next)
      MAILBOX()->device_response_queue_tail = 0;
  }
  return (uintptr_t)m;
}

uintptr_t hcall_trap(uintptr_t mcause, uintptr_t* regs)
{
  if (EXTRACT_FIELD(read_csr(mstatus), MSTATUS_PRV1) < PRV_S)
    return -1;

  uintptr_t n = regs[10], arg0 = regs[11], retval;
  switch (n)
  {
    case HCALL_HART_ID:
      retval = 0; // TODO
      break;
    case HCALL_CONSOLE_PUTCHAR:
      retval = hcall_console_putchar(arg0);
      break;
    case HCALL_SEND_DEVICE_REQUEST:
      retval = hcall_dev_req((sbi_device_message*)arg0);
      break;
    case HCALL_RECEIVE_DEVICE_RESPONSE:
      retval = hcall_dev_resp();
      break;
    default:
      retval = -ENOSYS;
      break;
  }
  regs[10] = retval;
  write_csr(mepc, read_csr(mepc) + 4);
  return 0;
}

uintptr_t machine_page_fault(uintptr_t mcause, uintptr_t* regs)
{
  // See if this trap occurred when emulating an instruction on behalf of
  // a lower privilege level.
  extern int32_t unprivileged_access_ranges[];
  extern int32_t unprivileged_access_ranges_end[];
  uintptr_t mepc = read_csr(mepc);

  int32_t* p = unprivileged_access_ranges;
  do {
    if (mepc >= p[0] && mepc < p[1]) {
      // Yes.  Skip to the end of the unprivileged access region.
      // Mark t0 zero so the emulation routine knows this occurred.
      regs[5] = 0;
      write_csr(mepc, p[1]);
      return 0;
    }
    p += 2;
  } while (p < unprivileged_access_ranges_end);

  // No.  We're boned.
  bad_trap();
}
