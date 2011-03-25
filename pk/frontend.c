#include "pk.h"
#include "atomic.h"
#include "frontend.h"
#include "pcr.h"
#include <stdint.h>

sysret_t frontend_syscall(long n, long a0, long a1, long a2, long a3)
{
  static volatile uint64_t magic_mem[8];

  static spinlock_t lock = SPINLOCK_INIT;
  spinlock_lock(&lock);

  magic_mem[0] = n;
  magic_mem[1] = a0;
  magic_mem[2] = a1;
  magic_mem[3] = a2;
  magic_mem[4] = a3;

  asm volatile ("fence");

  mtpcr(magic_mem,PCR_TOHOST);
  while(mfpcr(PCR_FROMHOST) == 0);

  sysret_t ret = {magic_mem[0],magic_mem[1]};

  spinlock_unlock(&lock);
  return ret;
}
