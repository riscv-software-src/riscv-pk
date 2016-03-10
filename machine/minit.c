#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "fp_emulation.h"
#include <string.h>

pte_t* root_page_table;
uintptr_t first_free_paddr;
uintptr_t mem_size;
uint32_t num_harts;

static void mstatus_init()
{
  uintptr_t ms = 0;
  ms = INSERT_FIELD(ms, MSTATUS_VM, VM_CHOICE);
  ms = INSERT_FIELD(ms, MSTATUS_FS, 1);
  write_csr(mstatus, ms);

  ms = read_csr(mstatus);
  assert(EXTRACT_FIELD(ms, MSTATUS_VM) == VM_CHOICE);

  write_csr(mtimecmp, 0);
  clear_csr(mip, MIP_MSIP);
  write_csr(mie, -1);
  write_csr(mucounteren, -1);
  write_csr(mscounteren, -1);
}

static void delegate_traps()
{
  uintptr_t interrupts = MIP_SSIP | MIP_STIP;
  uintptr_t exceptions =
    (1U << CAUSE_MISALIGNED_FETCH) |
    (1U << CAUSE_FAULT_FETCH) |
    (1U << CAUSE_BREAKPOINT) |
    (1U << CAUSE_FAULT_LOAD) |
    (1U << CAUSE_FAULT_STORE) |
    (1U << CAUSE_BREAKPOINT) |
    (1U << CAUSE_USER_ECALL);

  write_csr(mideleg, interrupts);
  write_csr(medeleg, exceptions);
  assert(read_csr(mideleg) == interrupts);
  assert(read_csr(medeleg) == exceptions);
}

static void fp_init()
{
  assert(read_csr(mstatus) & MSTATUS_FS);

#ifdef __riscv_hard_float
  if (!supports_extension('D'))
    die("FPU not found; recompile pk with -msoft-float");
  for (int i = 0; i < 32; i++)
    init_fp_reg(i);
  write_csr(fcsr, 0);
#else
  if (supports_extension('D'))
    die("FPU unexpectedly found; recompile with -mhard-float");
#endif
}

void hls_init(uint32_t id, csr_t* csrs)
{
  hls_t* hls = OTHER_HLS(id);
  memset(hls, 0, sizeof(*hls));
  hls->csrs = csrs;
}

static uintptr_t sbi_top_paddr()
{
  extern char _end;
  return ROUNDUP((uintptr_t)&_end, RISCV_PGSIZE);
}

static void memory_init()
{
  mem_size = mem_size / MEGAPAGE_SIZE * MEGAPAGE_SIZE;
  first_free_paddr = sbi_top_paddr() + num_harts * RISCV_PGSIZE;
}

static void hart_init()
{
  mstatus_init();
  fp_init();
  delegate_traps();
}

void init_first_hart()
{
  hart_init();
  memset(HLS(), 0, sizeof(*HLS()));
  parse_config_string();
  memory_init();
  boot_loader();
}

void init_other_hart()
{
  hart_init();

  // wait until hart 0 discovers us
  while (*(csr_t * volatile *)&HLS()->csrs == NULL)
    ;

  boot_other_hart();
}

void enter_supervisor_mode(void (*fn)(uintptr_t), uintptr_t stack)
{
  uintptr_t mstatus = read_csr(mstatus);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPP, PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPIE, 0);
  write_csr(mstatus, mstatus);
  write_csr(mscratch, MACHINE_STACK_TOP() - MENTRY_FRAME_SIZE);
  write_csr(mepc, fn);
  write_csr(sptbr, (uintptr_t)root_page_table >> RISCV_PGSHIFT);
  asm volatile ("mv a0, %0; mv sp, %0; eret" : : "r" (stack));
  __builtin_unreachable();
}
