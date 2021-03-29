// See LICENSE for license details.

#include "pk.h"
#include "mmap.h"
#include "file.h"
#include "frontend.h"
#include "bits.h"
#include <stdint.h>
#include <stdarg.h>

static void vprintk(const char* s, va_list vl)
{
  char out[256]; // XXX
  int res = vsnprintf(out, sizeof(out), s, vl);
  int size = MIN(res, sizeof(out));
  frontend_syscall(SYS_write, 2, kva2pa(out), size, 0, 0, 0, 0);
}

void printk(const char* s, ...)
{
  va_list vl;
  va_start(vl, s);

  vprintk(s, vl);

  va_end(vl);
}

static const char* get_regname(int r)
{
  static const char regnames[] = {
    "z \0" "ra\0" "sp\0" "gp\0" "tp\0" "t0\0"  "t1\0"  "t2\0"
    "s0\0" "s1\0" "a0\0" "a1\0" "a2\0" "a3\0"  "a4\0"  "a5\0"
    "a6\0" "a7\0" "s2\0" "s3\0" "s4\0" "s5\0"  "s6\0"  "s7\0"
    "s8\0" "s9\0" "sA\0" "sB\0" "t3\0" "t4\0"  "t5\0"  "t6"
  };

  return &regnames[r * 3];
}

void dump_tf(trapframe_t* tf)
{

  tf->gpr[0] = 0;

  for(int i = 0; i < 32; i+=4)
  {
    for(int j = 0; j < 4; j++)
      printk("%s %lx%c", get_regname(i+j), tf->gpr[i+j], j < 3 ? ' ' : '\n');
  }
  printk("pc %lx va/inst %lx sr %lx\n", tf->epc, tf->badvaddr, tf->status);
}

void do_panic(const char* s, ...)
{
  va_list vl;
  va_start(vl, s);

  vprintk(s, vl);
  shutdown(-1);

  va_end(vl);
}

void kassert_fail(const char* s)
{
  register uintptr_t ra asm ("ra");
  do_panic("assertion failed @ %p: %s\n", ra, s);
}
