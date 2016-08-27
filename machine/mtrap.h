#ifndef _RISCV_MTRAP_H
#define _RISCV_MTRAP_H

#include "encoding.h"

#ifdef __riscv_atomic
# define MAX_HARTS 8 // arbitrary
#else
# define MAX_HARTS 1
#endif

#ifndef __ASSEMBLER__

#include "sbi.h"
#include <stdint.h>
#include <stddef.h>

#define read_const_csr(reg) ({ unsigned long __tmp; \
  asm ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

static inline int supports_extension(char ext)
{
  return read_const_csr(misa) & (1 << (ext - 'A'));
}

static inline int xlen()
{
  return read_const_csr(misa) < 0 ? 64 : 32;
}

extern uintptr_t first_free_paddr;
extern uintptr_t mem_size;
extern uintptr_t num_harts;
extern volatile uint64_t* mtime;
extern volatile uint32_t* plic_priorities;
extern size_t plic_ndevs;

typedef struct {
  uint64_t* timecmp;
  uint32_t* ipi;
  volatile int mipi_pending;
  volatile int sipi_pending;
  int console_ibuf;

  volatile uint32_t* plic_m_thresh;
  volatile uintptr_t* plic_m_ie;
  volatile uint32_t* plic_s_thresh;
  volatile uintptr_t* plic_s_ie;
} hls_t;

#define IPI_SOFT      0x1
#define IPI_FENCE_I   0x2
#define IPI_SFENCE_VM 0x4

#define MACHINE_STACK_TOP() ({ \
  register uintptr_t sp asm ("sp"); \
  (void*)((sp + RISCV_PGSIZE) & -RISCV_PGSIZE); })

// hart-local storage, at top of stack
#define HLS() ((hls_t*)(MACHINE_STACK_TOP() - HLS_SIZE))
#define OTHER_HLS(id) ((hls_t*)((void*)HLS() + RISCV_PGSIZE * ((id) - read_const_csr(mhartid))))

hls_t* hls_init(uintptr_t hart_id);
void parse_config_string();
void poweroff(void) __attribute((noreturn));
void printm(const char* s, ...);
void putstring(const char* s);
#define assert(x) ({ if (!(x)) die("assertion failed: %s", #x); })
#define die(str, ...) ({ printm("%s:%d: " str "\n", __FILE__, __LINE__, ##__VA_ARGS__); poweroff(); })

void enter_supervisor_mode(void (*fn)(uintptr_t), uintptr_t stack)
  __attribute__((noreturn));
void boot_loader();
void boot_other_hart();

static inline void wfi()
{
  asm volatile ("wfi" ::: "memory");
}

#endif // !__ASSEMBLER__

#define MACHINE_STACK_SIZE RISCV_PGSIZE
#define MENTRY_FRAME_SIZE (INTEGER_CONTEXT_SIZE + SOFT_FLOAT_CONTEXT_SIZE \
                           + HLS_SIZE)

#ifdef __riscv_hard_float
# define SOFT_FLOAT_CONTEXT_SIZE 0
#else
# define SOFT_FLOAT_CONTEXT_SIZE (8 * 32)
#endif
#define HLS_SIZE 64
#define INTEGER_CONTEXT_SIZE (32 * REGBYTES)

#endif
