#include "vm.h"
#include "mtrap.h"
#include "devicetree.h"

volatile uint32_t booted_harts_mask;
uintptr_t mem_size;
uint32_t num_harts;

static void mstatus_init()
{
  if (!supports_extension('S'))
    panic("supervisor support is required");

  uintptr_t ms = 0;
  ms = INSERT_FIELD(ms, MSTATUS_PRV, PRV_M);
  ms = INSERT_FIELD(ms, MSTATUS_PRV1, PRV_S);
  ms = INSERT_FIELD(ms, MSTATUS_PRV2, PRV_U);
  ms = INSERT_FIELD(ms, MSTATUS_IE2, 1);
  ms = INSERT_FIELD(ms, MSTATUS_VM, VM_CHOICE);
  ms = INSERT_FIELD(ms, MSTATUS_FS, 3);
  ms = INSERT_FIELD(ms, MSTATUS_XS, 3);
  write_csr(mstatus, ms);
  ms = read_csr(mstatus);

  if (EXTRACT_FIELD(ms, MSTATUS_VM) != VM_CHOICE)
    have_vm = 0;

  write_csr(mtimecmp, 0);
  clear_csr(mip, MIP_MSIP);
  set_csr(mie, MIP_MSIP);
}

static void memory_init()
{
  if (mem_size == 0)
    panic("could not determine memory capacity");

  if (num_harts == 0)
    panic("could not determine number of harts");
}

static void fp_init()
{
  kassert(read_csr(mstatus) & MSTATUS_FS);

#ifdef __riscv_hard_float
  if (!supports_extension('D'))
    panic("FPU not found; recompile pk with -msoft-float");
  for (int i = 0; i < 32; i++)
    init_fp_reg(i);
  write_csr(fcsr, 0);
#else
  if (supports_extension('D'))
    panic("FPU unexpectedly found; recompile pk without -msoft-float");
#endif
}

void hls_init(uint32_t id, uintptr_t* csrs)
{
  hls_t* hls = OTHER_HLS(id);
  memset(hls, 0, sizeof(*hls));
  hls->hart_id = id;
  hls->csrs = csrs;

  if (id != 0) {
    while (((booted_harts_mask >> id) & 1) == 0)
      ;
    mb();
    // wake up the hart by granting it a stack
    csrs[CSR_MSCRATCH] = (uintptr_t)(OTHER_STACK_TOP(id) - MENTRY_FRAME_SIZE);
  }
}

static void init_hart()
{
  mstatus_init();
  fp_init();
}

void init_first_hart()
{
  init_hart();

  memset(HLS(), 0, sizeof(*HLS()));
  file_init();
  parse_device_tree();

  struct mainvars arg_buffer;
  struct mainvars *args = parse_args(&arg_buffer);

  memory_init();
  vm_init();
  boot_loader(args);
}

void init_other_hart()
{
  init_hart();

  // wait until virtual memory is enabled
  while (*(pte_t* volatile*)&root_page_table == NULL)
    ;
  mb();
  write_csr(sptbr, root_page_table);

  boot_other_hart();
}
