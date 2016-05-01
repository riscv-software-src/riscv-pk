#include "mtrap.h"
#include "sbi.h"

uintptr_t __sbi_query_memory(uintptr_t id, memory_block_info *p)
{
  if (id == 0) {
    p->base = first_free_paddr;
    p->size = mem_size + DRAM_BASE - p->base;
    return 0;
  }

  return -1;
}

#define LOW_IRQ_OK(n) ((n) == IRQ_S_SOFT || (n) == IRQ_S_TIMER)

uintptr_t __sbi_mask_interrupt(uintptr_t which)
{
  if (!LOW_IRQ_OK(which))
    return -1;

  clear_csr(sie, 1UL << which);
  return 0;
}

uintptr_t __sbi_unmask_interrupt(uintptr_t which)
{
  if (!LOW_IRQ_OK(which))
    return -1;

  set_csr(sie, 1UL << which);
  return 0;
}
