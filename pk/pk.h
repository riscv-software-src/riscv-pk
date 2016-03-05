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
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(a, lo, hi) MIN(MAX(a, lo), hi)

#ifdef __cplusplus
extern "C" {
#endif

extern int have_vm;

void printk(const char* s, ...);
void printm(const char* s, ...);
int vsnprintf(char* out, size_t n, const char* s, va_list vl);
int snprintf(char* out, size_t n, const char* s, ...);
void init_tf(trapframe_t*, long pc, long sp);
void start_user(trapframe_t* tf) __attribute__((noreturn));
void dump_tf(trapframe_t*);

void unhandled_trap(trapframe_t*);
void handle_misaligned_load(trapframe_t*);
void handle_misaligned_store(trapframe_t*);
void handle_fault_load(trapframe_t*);
void handle_fault_store(trapframe_t*);

static inline int insn_len(long insn)
{
  return (insn & 0x3) < 0x3 ? 2 : 4;
}

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define NUM_COUNTERS (18)
extern int uarch_counters_enabled;
extern long uarch_counters[NUM_COUNTERS];
extern char* uarch_counter_names[NUM_COUNTERS];

static inline void wfi()
{
  asm volatile ("wfi" ::: "memory");
}

#ifdef __cplusplus
}
#endif

#endif // !__ASSEMBLER__

#endif
