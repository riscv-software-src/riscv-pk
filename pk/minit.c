#include "vm.h"
#include "mtrap.h"

uintptr_t mem_size;
uint32_t num_harts;

static void mstatus_init()
{
  uintptr_t ms = read_csr(mstatus);
  ms = INSERT_FIELD(ms, MSTATUS_SA, UA_RV64);
  ms = INSERT_FIELD(ms, MSTATUS_UA, UA_RV64);
  ms = INSERT_FIELD(ms, MSTATUS_PRV1, PRV_S);
  ms = INSERT_FIELD(ms, MSTATUS_IE1, 0);
  ms = INSERT_FIELD(ms, MSTATUS_PRV2, PRV_U);
  ms = INSERT_FIELD(ms, MSTATUS_IE2, 1);
  ms = INSERT_FIELD(ms, MSTATUS_MPRV, PRV_M);
  ms = INSERT_FIELD(ms, MSTATUS_VM, VM_SV43);
  ms = INSERT_FIELD(ms, MSTATUS_FS, 3);
  ms = INSERT_FIELD(ms, MSTATUS_XS, 3);
  write_csr(mstatus, ms);
  ms = read_csr(mstatus);

  if (EXTRACT_FIELD(ms, MSTATUS_PRV1) != PRV_S) {
    ms = INSERT_FIELD(ms, MSTATUS_PRV1, PRV_U);
    ms = INSERT_FIELD(ms, MSTATUS_IE1, 1);
    write_csr(mstatus, ms);

    panic("supervisor support is required");
  }

  if (EXTRACT_FIELD(ms, MSTATUS_VM) != VM_SV43)
    have_vm = 0;
}

static void memory_init()
{
  if (mem_size == 0)
    panic("could not determine memory capacity");
}

static void hart_init()
{
  if (num_harts == 0)
    panic("could not determine number of harts");

  if (num_harts != 1)
    panic("TODO: SMP support");
}

void machine_init()
{
  file_init();

  struct mainvars arg_buffer;
  struct mainvars *args = parse_args(&arg_buffer);

  mstatus_init();
  memory_init();
  hart_init();
  vm_init();
  boot_loader(args);
}
