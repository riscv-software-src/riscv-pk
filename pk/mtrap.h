#ifndef _PK_MTRAP_H
#define _PK_MTRAP_H

#include "pk.h"
#include "bits.h"
#include "encoding.h"

#ifndef __ASSEMBLER__

#include "sbi.h"

#define GET_MACRO(_1,_2,_3,_4,NAME,...) NAME

#define unpriv_mem_access(a, b, c, ...) GET_MACRO(__VA_ARGS__, unpriv_mem_access3, unpriv_mem_access2, unpriv_mem_access1, unpriv_mem_access0)(a, b, c, __VA_ARGS__)
#define unpriv_mem_access0(a, b, c, d) ({ uintptr_t z = 0, z1 = 0, z2 = 0; unpriv_mem_access_base(a, b, c, d, z, z1, z2); })
#define unpriv_mem_access1(a, b, c, d, e) ({ uintptr_t z = 0, z1 = 0; unpriv_mem_access_base(a, b, c, d, e, z, z1); })
#define unpriv_mem_access2(a, b, c, d, e, f) ({ uintptr_t z = 0; unpriv_mem_access_base(a, b, c, d, e, f, z); })
#define unpriv_mem_access3(a, b, c, d, e, f, g) unpriv_mem_access_base(a, b, c, d, e, f, g)
#define unpriv_mem_access_base(code, o0, o1, o2, i0, i1, i2) ({ \
  register uintptr_t mstatus asm ("a3") = MSTATUS_MPRV; \
  register uintptr_t __mepc asm ("a2") = mepc; \
  uintptr_t unused1, unused2, unused3 __attribute__((unused)); \
  asm volatile ("csrrs %[mstatus], mstatus, %[mstatus]\n" \
                code "\n" \
                "csrw mstatus, %[mstatus]\n" \
                : [o0] "=&r"(o0), [o1] "=&r"(o1), [o2] "=&r"(o2), \
                  [mstatus] "+&r"(mstatus) \
                : [i0] "rJ"(i0), [i1] "rJ"(i1), [i2] "rJ"(i2), \
                  "r"(__mepc)); \
  (mstatus); \
})

typedef uint32_t insn_t;
typedef void (*emulation_func)(uintptr_t*, uintptr_t, uintptr_t, uintptr_t, insn_t);
#define DECLARE_EMULATION_FUNC(name) void name(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn)

void truly_illegal_insn(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn);
void redirect_trap(uintptr_t epc, uintptr_t mstatus);

#define GET_REG(insn, pos, regs) ({ \
  int mask = (1 << (5+LOG_REGBYTES)) - (1 << LOG_REGBYTES); \
  (uintptr_t*)((uintptr_t)regs + (((insn) >> ((pos) - LOG_REGBYTES)) & mask)); \
})
#define GET_RS1(insn, regs) (*GET_REG(insn, 15, regs))
#define GET_RS2(insn, regs) (*GET_REG(insn, 20, regs))
#define SET_RD(insn, regs, val) (*GET_REG(insn, 7, regs) = (val))
#define IMM_I(insn) ((int32_t)(insn) >> 20)
#define IMM_S(insn) (((int32_t)(insn) >> 25 << 5) | (int32_t)(((insn) >> 7) & 0x1f))
#define MASK_FUNCT3 0x7000

#define GET_PRECISION(insn) (((insn) >> 25) & 3)
#define GET_RM(insn) (((insn) >> 12) & 7)
#define PRECISION_S 0
#define PRECISION_D 1

#ifdef __riscv_hard_float
# define GET_F32_REG(insn, pos, regs) ({ \
  register int32_t value asm("a0") = ((insn) >> ((pos)-3)) & 0xf8; \
  uintptr_t tmp; \
  asm ("1: auipc %0, %%pcrel_hi(get_f32_reg); add %0, %0, %1; jalr t0, %0, %%pcrel_lo(1b)" : "=&r"(tmp), "+&r"(value) :: "t0"); \
  value; })
# define SET_F32_REG(insn, pos, regs, val) ({ \
  register uint32_t value asm("a0") = (val); \
  uintptr_t offset = ((insn) >> ((pos)-3)) & 0xf8; \
  uintptr_t tmp; \
  asm volatile ("1: auipc %0, %%pcrel_hi(put_f32_reg); add %0, %0, %2; jalr t0, %0, %%pcrel_lo(1b)" : "=&r"(tmp) : "r"(value), "r"(offset) : "t0"); })
# define init_fp_reg(i) SET_F32_REG((i) << 3, 3, 0, 0)
# define GET_F64_REG(insn, pos, regs) ({ \
  register uintptr_t value asm("a0") = ((insn) >> ((pos)-3)) & 0xf8; \
  uintptr_t tmp; \
  asm ("1: auipc %0, %%pcrel_hi(get_f64_reg); add %0, %0, %1; jalr t0, %0, %%pcrel_lo(1b)" : "=&r"(tmp), "+&r"(value) :: "t0"); \
  sizeof(uintptr_t) == 4 ? *(int64_t*)value : (int64_t)value; })
# define SET_F64_REG(insn, pos, regs, val) ({ \
  uint64_t __val = (val); \
  register uintptr_t value asm("a0") = sizeof(uintptr_t) == 4 ? (uintptr_t)&__val : (uintptr_t)__val; \
  uintptr_t offset = ((insn) >> ((pos)-3)) & 0xf8; \
  uintptr_t tmp; \
  asm volatile ("1: auipc %0, %%pcrel_hi(put_f64_reg); add %0, %0, %2; jalr t0, %0, %%pcrel_lo(1b)" : "=&r"(tmp) : "r"(value), "r"(offset) : "t0"); })
# define GET_FCSR() read_csr(fcsr)
# define SET_FCSR(value) write_csr(fcsr, (value))
# define GET_FRM() read_csr(frm)
# define SET_FRM(value) write_csr(frm, (value))
# define GET_FFLAGS() read_csr(fflags)
# define SET_FFLAGS(value) write_csr(fflags, (value))

# define SETUP_STATIC_ROUNDING(insn) ({ \
  register long tp asm("tp") = read_csr(frm); \
  if (likely(((insn) & MASK_FUNCT3) == MASK_FUNCT3)) ; \
  else if (GET_RM(insn) > 4) return truly_illegal_insn(regs, mcause, mepc, mstatus, insn); \
  else tp = GET_RM(insn); \
  asm volatile ("":"+r"(tp)); })
# define softfloat_raiseFlags(which) set_csr(fflags, which)
# define softfloat_roundingMode ({ register int tp asm("tp"); tp; })
#else
# define GET_F64_REG(insn, pos, regs) (((int64_t*)(&(regs)[32]))[((insn) >> (pos)) & 0x1f])
# define SET_F64_REG(insn, pos, regs, val) (GET_F64_REG(insn, pos, regs) = (val))
# define GET_F32_REG(insn, pos, regs) (*(int32_t*)&GET_F64_REG(insn, pos, regs))
# define SET_F32_REG(insn, pos, regs, val) (GET_F32_REG(insn, pos, regs) = (val))
# define GET_FCSR() ({ register int tp asm("tp"); tp & 0xFF; })
# define SET_FCSR(value) ({ asm volatile("add tp, x0, %0" :: "rI"((value) & 0xFF)); })
# define GET_FRM() (GET_FCSR() >> 5)
# define SET_FRM(value) SET_FCSR(GET_FFLAGS() | ((value) << 5))
# define GET_FFLAGS() (GET_FCSR() & 0x1F)
# define SET_FFLAGS(value) SET_FCSR((GET_FRM() << 5) | ((value) & 0x1F))

# define SETUP_STATIC_ROUNDING(insn) ({ \
  register int tp asm("tp"); tp &= 0xFF; \
  if (likely(((insn) & MASK_FUNCT3) == MASK_FUNCT3)) tp |= tp << 8; \
  else if (GET_RM(insn) > 4) return truly_illegal_insn(regs, mcause, mepc, mstatus, insn); \
  else tp |= GET_RM(insn) << 13; \
  asm volatile ("":"+r"(tp)); })
# define softfloat_raiseFlags(which) ({ asm volatile ("or tp, tp, %0" :: "rI"(which)); })
# define softfloat_roundingMode ({ register int tp asm("tp"); tp >> 13; })
#endif

#define GET_F32_RS1(insn, regs) (GET_F32_REG(insn, 15, regs))
#define GET_F32_RS2(insn, regs) (GET_F32_REG(insn, 20, regs))
#define GET_F32_RS3(insn, regs) (GET_F32_REG(insn, 27, regs))
#define GET_F64_RS1(insn, regs) (GET_F64_REG(insn, 15, regs))
#define GET_F64_RS2(insn, regs) (GET_F64_REG(insn, 20, regs))
#define GET_F64_RS3(insn, regs) (GET_F64_REG(insn, 27, regs))
#define SET_F32_RD(insn, regs, val) (SET_F32_REG(insn, 7, regs, val), SET_FS_DIRTY())
#define SET_F64_RD(insn, regs, val) (SET_F64_REG(insn, 7, regs, val), SET_FS_DIRTY())
#define SET_FS_DIRTY() set_csr(mstatus, MSTATUS_FS)

static insn_t __attribute__((always_inline)) get_insn(uintptr_t mepc, uintptr_t* mstatus)
{
  insn_t insn;

#ifdef __riscv_compressed
  int rvc_mask = 3, insn_hi;
  *mstatus = unpriv_mem_access("lhu %[insn], 0(%[mepc]);"
                               "and %[insn_hi], %[insn], %[rvc_mask];"
                               "bne %[insn_hi], %[rvc_mask], 1f;"
                               "lh %[insn_hi], 2(%[mepc]);"
                               "sll %[insn_hi], %[insn_hi], 16;"
                               "or %[insn], %[insn], %[insn_hi];"
                               "1:",
                               insn, insn_hi, unused1, mepc, rvc_mask);
#else
  *mstatus = unpriv_mem_access("lw %[insn], 0(%[mepc])",
                               insn, unused1, unused2, mepc);
#endif

  return insn;
}

static inline long __attribute__((pure)) cpuid()
{
  long res;
  asm ("csrr %0, mcpuid" : "=r"(res)); // not volatile, so don't use read_csr()
  return res;
}

static inline int supports_extension(char ext)
{
  return cpuid() & (1 << (ext - 'A'));
}

static inline int xlen()
{
  return cpuid() < 0 ? 64 : 32;
}

typedef struct {
  sbi_device_message* device_request_queue_head;
  size_t device_request_queue_size;
  sbi_device_message* device_response_queue_head;
  sbi_device_message* device_response_queue_tail;

  volatile uintptr_t* csrs;
  int hart_id;
  volatile int ipi_pending;
} hls_t;

#define IPI_SOFT      0x1
#define IPI_FENCE_I   0x2
#define IPI_SFENCE_VM 0x4

void hls_init(uint32_t hart_id, uintptr_t* csrs);

#define MACHINE_STACK_TOP() ({ \
  register uintptr_t sp asm ("sp"); \
  (void*)((sp + RISCV_PGSIZE) & -RISCV_PGSIZE); })

// hart-local storage, at top of stack
#define HLS() ((hls_t*)(MACHINE_STACK_TOP() - HLS_SIZE))
#define OTHER_STACK_TOP(id) (MACHINE_STACK_TOP() + RISCV_PGSIZE * ((id) - HLS()->hart_id))
#define OTHER_HLS(id) ((hls_t*)((void*)HLS() + RISCV_PGSIZE * ((id) - HLS()->hart_id)))

#define printk printm

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
