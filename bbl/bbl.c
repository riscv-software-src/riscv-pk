// See LICENSE for license details.

#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include "fdt.h"
#include <string.h>

extern char _payload_start, _payload_end; /* internal payload */
static const void* entry_point;
long disabled_hart_mask;

static uintptr_t dtb_output()
{
  /*
   * Place DTB after the payload, either the internal payload or a
   * preloaded external payload specified in device-tree, if present.
   *
   * Note: linux kernel calls __va(dtb) to get the device-tree virtual
   * address. The kernel's virtual mapping begins at its load address,
   * thus mandating device-tree is in physical memory after the kernel.
   */
  uintptr_t end = kernel_end ? (uintptr_t)kernel_end : (uintptr_t)&_payload_end;
  return (end + MEGAPAGE_SIZE - 1) / MEGAPAGE_SIZE * MEGAPAGE_SIZE;
}

static void filter_dtb(uintptr_t source)
{
  uintptr_t dest = dtb_output();
  uint32_t size = fdt_size(source);
  memcpy((void*)dest, (void*)source, size);

  // Remove information from the chained FDT
  filter_harts(dest, &disabled_hart_mask);
  filter_plic(dest);
  filter_compat(dest, "riscv,clint0");
  filter_compat(dest, "riscv,debug-013");
}

void boot_other_hart(uintptr_t unused __attribute__((unused)))
{
  const void* entry;
  do {
    entry = entry_point;
    mb();
  } while (!entry);

  long hartid = read_csr(mhartid);
  if ((1 << hartid) & disabled_hart_mask) {
    while (1) {
      __asm__ volatile("wfi");
#ifdef __riscv_div
      __asm__ volatile("div x0, x0, x0");
#endif
    }
  }

#ifdef BBL_BOOT_MACHINE
  enter_machine_mode(entry, hartid, dtb_output());
#else /* Run bbl in supervisor mode */
  enter_supervisor_mode(entry, hartid, dtb_output());
#endif
}

void boot_loader(uintptr_t dtb)
{
  filter_dtb(dtb);
#ifdef PK_ENABLE_LOGO
  print_logo();
#endif
#ifdef PK_PRINT_DEVICE_TREE
  fdt_print(dtb_output());
#endif
  mb();
  /* Use optional FDT preloaded external payload if present */
  entry_point = kernel_start ? kernel_start : &_payload_start;
  boot_other_hart(0);
}
