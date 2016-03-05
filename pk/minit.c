#include "vm.h"
#include "mtrap.h"
#include "fp_emulation.h"
#include "boot.h"

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

static void hart_init()
{
  mstatus_init();
  fp_init();
  delegate_traps();
}

void init_first_hart()
{
  file_init();
  hart_init();

  memset(HLS(), 0, sizeof(*HLS()));
  parse_config_string();
  vm_init();
  boot_loader();
}

void init_other_hart()
{
  hart_init();

  // wait until virtual memory is enabled
  while (*(pte_t* volatile*)&root_page_table == NULL)
    ;
  mb();
  write_csr(sptbr, (uintptr_t)root_page_table >> RISCV_PGSHIFT);

  // make sure hart 0 has discovered us
  assert(HLS()->csrs != NULL);

  boot_other_hart();
}

void prepare_supervisor_mode()
{
  uintptr_t mstatus = read_csr(mstatus);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPP, PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPIE, 0);
  write_csr(mstatus, mstatus);
  write_csr(mscratch, MACHINE_STACK_TOP() - MENTRY_FRAME_SIZE);
}
