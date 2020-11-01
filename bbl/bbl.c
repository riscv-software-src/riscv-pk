// See LICENSE for license details.

#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include "fdt.h"
#include <string.h>

#ifdef BBL_PAYLOAD
extern char _payload_start, _payload_end; /* internal payload */
# define PAYLOAD_START &_payload_start
# define PAYLOAD_END ROUNDUP((uintptr_t)&_payload_end, MEGAPAGE_SIZE)
#else
# define PAYLOAD_START (void*)(MEM_START + MEGAPAGE_SIZE)
# define PAYLOAD_END (void*)(MEM_START + 0x2200000)
#endif
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
  uintptr_t end = kernel_end ? ROUNDUP((uintptr_t)kernel_end, MEGAPAGE_SIZE)
                             : (uintptr_t)PAYLOAD_END;
  return end;
}

static void filter_dtb(uintptr_t source)
{
  uintptr_t dest = dtb_output();
  uint32_t size = fdt_size(source);
  memcpy((void*)dest, (void*)source, size);

#ifndef CUSTOM_DTS
  // Remove information from the chained FDT
  filter_harts(dest, &disabled_hart_mask);
  filter_plic(dest);
  filter_compat(dest, "riscv,clint0");
  filter_compat(dest, "riscv,debug-013");
#endif
}

static void protect_memory(void)
{
  // Check to see if up to four PMP registers are implemented.
  // Ignore the illegal-instruction trap if PMPs aren't supported.
  uintptr_t a0 = 0, a1 = 0, a2 = 0, a3 = 0, tmp, cfg;
  asm volatile ("la %[tmp], 1f\n\t"
                "csrrw %[tmp], mtvec, %[tmp]\n\t"
                "csrw pmpaddr0, %[m1]\n\t"
                "csrr %[a0], pmpaddr0\n\t"
                "csrw pmpaddr1, %[m1]\n\t"
                "csrr %[a1], pmpaddr1\n\t"
                "csrw pmpaddr2, %[m1]\n\t"
                "csrr %[a2], pmpaddr2\n\t"
                "csrw pmpaddr3, %[m1]\n\t"
                "csrr %[a3], pmpaddr3\n\t"
                ".align 2\n\t"
                "1: csrw mtvec, %[tmp]"
                : [tmp] "=&r" (tmp),
                  [a0] "+r" (a0), [a1] "+r" (a1), [a2] "+r" (a2), [a3] "+r" (a3)
                : [m1] "r" (-1UL));

  // We need at least four PMP registers to protect M-mode from S-mode.
  if (!(a0 & a1 & a2 & a3))
    return setup_pmp();

  // Prevent S-mode access to our part of memory.
  extern char _ftext, _end;
  a0 = (uintptr_t)&_ftext >> PMP_SHIFT;
  a1 = (uintptr_t)&_end >> PMP_SHIFT;
  cfg = PMP_TOR << 8;
  // Give S-mode free rein of everything else.
  a2 = -1;
  cfg |= (PMP_NAPOT | PMP_R | PMP_W | PMP_X) << 16;
  // No use for PMP 3 just yet.
  a3 = 0;

  // Plug it all in.
  asm volatile ("csrw pmpaddr0, %[a0]\n\t"
                "csrw pmpaddr1, %[a1]\n\t"
                "csrw pmpaddr2, %[a2]\n\t"
                "csrw pmpaddr3, %[a3]\n\t"
                "csrw pmpcfg0, %[cfg]"
                :: [a0] "r" (a0), [a1] "r" (a1), [a2] "r" (a2), [a3] "r" (a3),
                   [cfg] "r" (cfg));
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
  protect_memory();
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
  entry_point = kernel_start ? kernel_start : PAYLOAD_START;
  boot_other_hart(0);
}
