#include "pk.h"
#include "vm.h"
#include "frontend.h"
#include "sbi.h"
#include "mcall.h"
#include <errno.h>

#define sbi_printk(str, ...) ({ \
  char buf[1024]; /* XXX */ \
  sprintk(buf, str, __VA_ARGS__); \
  for (size_t i = 0; buf[i]; i++) \
    do_mcall(MCALL_CONSOLE_PUTCHAR, buf[i]); })

uintptr_t __sbi_query_memory(uintptr_t id, memory_block_info *p)
{
  if (id == 0) {
    p->base = current.first_free_paddr;
    p->size = mem_size - p->base;
    return 0;
  }

  return -1;
}
