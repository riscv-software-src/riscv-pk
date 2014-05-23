// See LICENSE for license details.

#include "pk.h"
#include "atomic.h"
#include "frontend.h"
#include <stdint.h>

long frontend_syscall(long n, long a0, long a1, long a2, long a3, long a4)
{
  static volatile uint64_t magic_mem[8];

  static spinlock_t lock = SPINLOCK_INIT;
  spinlock_lock(&lock);

  magic_mem[0] = n;
  magic_mem[1] = a0;
  magic_mem[2] = a1;
  magic_mem[3] = a2;
  magic_mem[4] = a3;
  magic_mem[5] = a4;

  mb();

  write_csr(tohost, magic_mem);
  while (swap_csr(fromhost, 0) == 0);

  mb();

  long ret = magic_mem[0];

  spinlock_unlock(&lock);
  return ret;
}
