// See LICENSE for license details.

#ifndef _PK_H
#define _PK_H

#ifndef __ASSEMBLER__

#include "encoding.h"
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

typedef struct
{
  long gpr[32];
  long status;
  long epc;
  long badvaddr;
  long cause;
  long insn;
} trapframe_t;

#define panic(s,...) do { do_panic(s"\n", ##__VA_ARGS__); } while(0)
#define kassert(cond) do { if(!(cond)) kassert_fail(""#cond); } while(0)
void do_panic(const char* s, ...) __attribute__((noreturn));
void kassert_fail(const char* s) __attribute__((noreturn));

#ifdef __cplusplus
extern "C" {
#endif

void printk(const char* s, ...);
void printm(const char* s, ...);
int vsnprintf(char* out, size_t n, const char* s, va_list vl);
int snprintf(char* out, size_t n, const char* s, ...);
void start_user(trapframe_t* tf) __attribute__((noreturn));
void dump_tf(trapframe_t*);

static inline int insn_len(long insn)
{
  return (insn & 0x3) < 0x3 ? 2 : 4;
}

#if __riscv_xlen == 32

static inline uint64_t rdtime64()
{
  uint32_t time;
  uint32_t timeh1;
  uint32_t timeh2;

  do
  {
    timeh1 = read_csr(timeh);
    time = read_csr(time);
    timeh2 = read_csr(timeh);
  } while(timeh1 != timeh2);

  return (((uint64_t) timeh1) << 32) | time;
}

static inline uint64_t rdcycle64()
{
  uint32_t cycle;
  uint32_t cycleh1;
  uint32_t cycleh2;

  do
  {
    cycleh1 = read_csr(cycleh);
    cycle = read_csr(cycle);
    cycleh2 = read_csr(cycleh);
  } while(cycleh1 != cycleh2);

  return (((uint64_t) cycleh1) << 32) | cycle;
}

static inline uint64_t rdinstret64()
{
  uint32_t instret;
  uint32_t instreth1;
  uint32_t instreth2;

  do
  {
    instreth1 = read_csr(instreth);
    instret = read_csr(instret);
    instreth2 = read_csr(instreth);
  } while(instreth1 != instreth2);

  return (((uint64_t) instreth1) << 32) | instret;
}

#else

#define rdtime64 rdtime
#define rdcycle64 rdcycle
#define rdinstret64 rdinstret

#endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#ifdef __cplusplus
}
#endif

#endif // !__ASSEMBLER__

#endif
