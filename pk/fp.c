// See LICENSE for license details.

#include "pk.h"
#include "fp.h"
#include "config.h"

#ifdef PK_ENABLE_FP_EMULATION

#include "softfloat.h"
#include <stdint.h>

#define noisy 0

static inline void
validate_address(trapframe_t* tf, long addr, int size, int store)
{
}

#ifdef __riscv_hard_float
# define get_fcsr() ({ fcsr_t fcsr; asm ("frcsr %0" : "=r"(fcsr)); fcsr; })
# define put_fcsr(value) ({ asm ("fscsr %0" :: "r"(value)); })
# define get_f32_reg(i) ({ \
  register int value asm("a0"); \
  register long offset asm("a1") = (i) * 8; \
  asm ("1: auipc %0, %%pcrel_hi(get_f32_reg); add %0, %0, %1; jalr %0, %%pcrel_lo(1b)" : "=&r"(value) : "r"(offset)); \
  value; })
# define put_f32_reg(i, value) ({ \
  long tmp; \
  register long __value asm("a0") = (value); \
  register long offset asm("a1") = (i) * 8; \
  asm ("1: auipc %0, %%pcrel_hi(put_f32_reg); add %0, %0, %1; jalr %0, %%pcrel_lo(1b)" : "=&r"(tmp) : "r"(offset), "r"(__value)); })
# ifdef __riscv64
#  define get_f64_reg(i) ({ \
  register long value asm("a0"); \
  register long offset asm("a1") = (i) * 8; \
  asm ("1: auipc %0, %%pcrel_hi(get_f64_reg); add %0, %0, %1; jalr %0, %%pcrel_lo(1b)" : "=&r"(value) : "r"(offset)); \
  value; })
#  define put_f64_reg(i, value) ({ \
  long tmp; \
  register long __value asm("a0") = (value); \
  register long offset asm("a1") = (i) * 8; \
  asm ("1: auipc %0, %%pcrel_hi(put_f64_reg); add %0, %0, %1; jalr %0, %%pcrel_lo(1b)" : "=&r"(tmp) : "r"(offset), "r"(__value)); })
# else
#  define get_f64_reg(i) ({ \
  long long value; \
  register long long* valuep asm("a0") = &value; \
  register long offset asm("a1") = (i) * 8; \
  asm ("1: auipc %0, %%pcrel_hi(get_f64_reg); add %0, %0, %1; jalr %0, %%pcrel_lo(1b)" : "=&r"(valuep) : "r"(offset)); \
  value; })
#  define put_f64_reg(i, value) ({ \
  long long __value = (value); \
  register long long* valuep asm("a0") = &__value; \
  register long offset asm("a1") = (i) * 8; \
  asm ("1: auipc %0, %%pcrel_hi(put_f64_reg); add %0, %0, %1; jalr %0, %%pcrel_lo(1b)" : "=&r"(tmp) : "r"(offset), "r"(__value)); })
# endif
#else
static fp_state_t fp_state;
# define get_fcsr() fp_state.fcsr
# define put_fcsr(value) fp_state.fcsr = (value)
# define get_f32_reg(i) fp_state.fpr[i]
# define get_f64_reg(i) fp_state.fpr[i]
# define put_f32_reg(i, value) fp_state.fpr[i] = (value)
# define put_f64_reg(i, value) fp_state.fpr[i] = (value)
#endif

int emulate_fp(trapframe_t* tf)
{
  if(noisy)
    printk("FPU emulation at pc %lx, insn %x\n",tf->epc,(uint32_t)tf->insn);

  #define RS1 ((tf->insn >> 15) & 0x1F)
  #define RS2 ((tf->insn >> 20) & 0x1F)
  #define RS3 ((tf->insn >> 27) & 0x1F)
  #define RD  ((tf->insn >>  7) & 0x1F)
  #define RM  ((tf->insn >> 12) &  0x7)

  int32_t imm = (int32_t)tf->insn >> 20;
  int32_t bimm = RD | imm >> 5 << 5;

  #define XRS1 (tf->gpr[RS1])
  #define XRS2 (tf->gpr[RS2])
  #define XRDR (tf->gpr[RD])

  #define frs1d get_f64_reg(RS1)
  #define frs2d get_f64_reg(RS2)
  #define frs3d get_f64_reg(RS3)
  #define frs1s get_f32_reg(RS1)
  #define frs2s get_f32_reg(RS2)
  #define frs3s get_f32_reg(RS3)

  long effective_address_load = XRS1 + imm;
  long effective_address_store = XRS1 + bimm;

  fcsr_t fcsr = get_fcsr();
  softfloat_exceptionFlags = fcsr.fcsr.flags;
  softfloat_roundingMode = (RM == 7) ? fcsr.fcsr.rm : RM;

  #define IS_INSN(x) ((tf->insn & MASK_ ## x) == MATCH_ ## x)

  #define DO_WRITEBACK(dp, value) ({ \
    if (dp) put_f64_reg(RD, value); \
    else put_f32_reg(RD, value); })

  #define DO_CSR(which, op) ({ long tmp = which; which op; tmp; })

  if(IS_INSN(FDIV_S))
    DO_WRITEBACK(0, f32_div(frs1s, frs2s));
  else if(IS_INSN(FDIV_D))
    DO_WRITEBACK(1, f64_div(frs1d, frs2d));
  else if(IS_INSN(FSQRT_S))
    DO_WRITEBACK(0, f32_sqrt(frs1s));
  else if(IS_INSN(FSQRT_D))
    DO_WRITEBACK(1, f64_sqrt(frs1d));
  else if(IS_INSN(FLW))
  {
    validate_address(tf, effective_address_load, 4, 0);
    DO_WRITEBACK(0, *(uint32_t*)effective_address_load);
  }
  else if(IS_INSN(FLD))
  {
    validate_address(tf, effective_address_load, 8, 0);
    DO_WRITEBACK(1, *(uint64_t*)effective_address_load);
  }
  else if(IS_INSN(FSW))
  {
    validate_address(tf, effective_address_store, 4, 1);
    *(uint32_t*)effective_address_store = frs2s;
  }
  else if(IS_INSN(FSD))
  {
    validate_address(tf, effective_address_store, 8, 1);
    *(uint64_t*)effective_address_store = frs2d;
  }
  else if(IS_INSN(FMV_X_S))
    XRDR = frs1s;
  else if(IS_INSN(FMV_X_D))
    XRDR = frs1d;
  else if(IS_INSN(FMV_S_X))
    DO_WRITEBACK(0, XRS1);
  else if(IS_INSN(FMV_D_X))
    DO_WRITEBACK(1, XRS1);
  else if(IS_INSN(FSGNJ_S))
    DO_WRITEBACK(0, (frs1s &~ (uint32_t)INT32_MIN) | (frs2s & (uint32_t)INT32_MIN));
  else if(IS_INSN(FSGNJ_D))
    DO_WRITEBACK(1, (frs1d &~ INT64_MIN) | (frs2d & INT64_MIN));
  else if(IS_INSN(FSGNJN_S))
    DO_WRITEBACK(0, (frs1s &~ (uint32_t)INT32_MIN) | ((~frs2s) & (uint32_t)INT32_MIN));
  else if(IS_INSN(FSGNJN_D))
    DO_WRITEBACK(1, (frs1d &~ INT64_MIN) | ((~frs2d) & INT64_MIN));
  else if(IS_INSN(FSGNJX_S))
    DO_WRITEBACK(0, frs1s ^ (frs2s & (uint32_t)INT32_MIN));
  else if(IS_INSN(FSGNJX_D))
    DO_WRITEBACK(1, frs1d ^ (frs2d & INT64_MIN));
  else if(IS_INSN(FEQ_S))
    XRDR = f32_eq(frs1s, frs2s);
  else if(IS_INSN(FEQ_D))
    XRDR = f64_eq(frs1d, frs2d);
  else if(IS_INSN(FLE_S))
    XRDR = f32_eq(frs1s, frs2s) || f32_lt(frs1s, frs2s);
  else if(IS_INSN(FLE_D))
    XRDR = f64_eq(frs1d, frs2d) || f64_lt(frs1d, frs2d);
  else if(IS_INSN(FLT_S))
    XRDR = f32_lt(frs1s, frs2s);
  else if(IS_INSN(FLT_D))
    XRDR = f64_lt(frs1d, frs2d);
  else if(IS_INSN(FCVT_S_W))
    DO_WRITEBACK(0, i64_to_f32((int64_t)(int32_t)XRS1));
  else if(IS_INSN(FCVT_S_L))
    DO_WRITEBACK(0, i64_to_f32(XRS1));
  else if(IS_INSN(FCVT_S_D))
    DO_WRITEBACK(0, f64_to_f32(frs1d));
  else if(IS_INSN(FCVT_D_W))
    DO_WRITEBACK(1, i64_to_f64((int64_t)(int32_t)XRS1));
  else if(IS_INSN(FCVT_D_L))
    DO_WRITEBACK(1, i64_to_f64(XRS1));
  else if(IS_INSN(FCVT_D_S))
    DO_WRITEBACK(1, f32_to_f64(frs1s));
  else if(IS_INSN(FCVT_S_WU))
    DO_WRITEBACK(0, ui64_to_f32((uint64_t)(uint32_t)XRS1));
  else if(IS_INSN(FCVT_S_LU))
    DO_WRITEBACK(0, ui64_to_f32(XRS1));
  else if(IS_INSN(FCVT_D_WU))
    DO_WRITEBACK(1, ui64_to_f64((uint64_t)(uint32_t)XRS1));
  else if(IS_INSN(FCVT_D_LU))
    DO_WRITEBACK(1, ui64_to_f64(XRS1));
  else if(IS_INSN(FADD_S))
    DO_WRITEBACK(0, f32_mulAdd(frs1s, 0x3f800000, frs2s));
  else if(IS_INSN(FADD_D))
    DO_WRITEBACK(1, f64_mulAdd(frs1d, 0x3ff0000000000000LL, frs2d));
  else if(IS_INSN(FSUB_S))
    DO_WRITEBACK(0, f32_mulAdd(frs1s, 0x3f800000, frs2s ^ (uint32_t)INT32_MIN));
  else if(IS_INSN(FSUB_D))
    DO_WRITEBACK(1, f64_mulAdd(frs1d, 0x3ff0000000000000LL, frs2d ^ INT64_MIN));
  else if(IS_INSN(FMUL_S))
    DO_WRITEBACK(0, f32_mulAdd(frs1s, frs2s, 0));
  else if(IS_INSN(FMUL_D))
    DO_WRITEBACK(1, f64_mulAdd(frs1d, frs2d, 0));
  else if(IS_INSN(FMADD_S))
    DO_WRITEBACK(0, f32_mulAdd(frs1s, frs2s, frs3s));
  else if(IS_INSN(FMADD_D))
    DO_WRITEBACK(1, f64_mulAdd(frs1d, frs2d, frs3d));
  else if(IS_INSN(FMSUB_S))
    DO_WRITEBACK(0, f32_mulAdd(frs1s, frs2s, frs3s ^ (uint32_t)INT32_MIN));
  else if(IS_INSN(FMSUB_D))
    DO_WRITEBACK(1, f64_mulAdd(frs1d, frs2d, frs3d ^ INT64_MIN));
  else if(IS_INSN(FNMADD_S))
    DO_WRITEBACK(0, f32_mulAdd(frs1s, frs2s, frs3s) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(FNMADD_D))
    DO_WRITEBACK(1, f64_mulAdd(frs1d, frs2d, frs3d) ^ INT64_MIN);
  else if(IS_INSN(FNMSUB_S))
    DO_WRITEBACK(0, f32_mulAdd(frs1s, frs2s, frs3s ^ (uint32_t)INT32_MIN) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(FNMSUB_D))
    DO_WRITEBACK(1, f64_mulAdd(frs1d, frs2d, frs3d ^ INT64_MIN) ^ INT64_MIN);
  else if(IS_INSN(FCVT_W_S))
    XRDR = f32_to_i32(frs1s, softfloat_roundingMode, true);
  else if(IS_INSN(FCVT_W_D))
    XRDR = f64_to_i32(frs1d, softfloat_roundingMode, true);
  else if(IS_INSN(FCVT_L_S))
    XRDR = f32_to_i64(frs1s, softfloat_roundingMode, true);
  else if(IS_INSN(FCVT_L_D))
    XRDR = f64_to_i64(frs1d, softfloat_roundingMode, true);
  else if(IS_INSN(FCVT_WU_S))
    XRDR = f32_to_ui32(frs1s, softfloat_roundingMode, true);
  else if(IS_INSN(FCVT_WU_D))
    XRDR = f64_to_ui32(frs1d, softfloat_roundingMode, true);
  else if(IS_INSN(FCVT_LU_S))
    XRDR = f32_to_ui64(frs1s, softfloat_roundingMode, true);
  else if(IS_INSN(FCVT_LU_D))
    XRDR = f64_to_ui64(frs1d, softfloat_roundingMode, true);
  else if(IS_INSN(FCLASS_S))
    XRDR = f32_classify(frs1s);
  else if(IS_INSN(FCLASS_D))
    XRDR = f64_classify(frs1s);
  else if(IS_INSN(CSRRS) && imm == CSR_FCSR) XRDR = DO_CSR(fcsr.bits, |= XRS1);
  else if(IS_INSN(CSRRS) && imm == CSR_FRM) XRDR = DO_CSR(fcsr.fcsr.rm, |= XRS1);
  else if(IS_INSN(CSRRS) && imm == CSR_FFLAGS) XRDR = DO_CSR(fcsr.fcsr.flags, |= XRS1);
  else if(IS_INSN(CSRRSI) && imm == CSR_FCSR) XRDR = DO_CSR(fcsr.bits, |= RS1);
  else if(IS_INSN(CSRRSI) && imm == CSR_FRM) XRDR = DO_CSR(fcsr.fcsr.rm, |= RS1);
  else if(IS_INSN(CSRRSI) && imm == CSR_FFLAGS) XRDR = DO_CSR(fcsr.fcsr.flags, |= RS1);
  else if(IS_INSN(CSRRC) && imm == CSR_FCSR) XRDR = DO_CSR(fcsr.bits, &= ~XRS1);
  else if(IS_INSN(CSRRC) && imm == CSR_FRM) XRDR = DO_CSR(fcsr.fcsr.rm, &= ~XRS1);
  else if(IS_INSN(CSRRC) && imm == CSR_FFLAGS) XRDR = DO_CSR(fcsr.fcsr.flags, &= ~XRS1);
  else if(IS_INSN(CSRRCI) && imm == CSR_FCSR) XRDR = DO_CSR(fcsr.bits, &= ~RS1);
  else if(IS_INSN(CSRRCI) && imm == CSR_FRM) XRDR = DO_CSR(fcsr.fcsr.rm, &= ~RS1);
  else if(IS_INSN(CSRRCI) && imm == CSR_FFLAGS) XRDR = DO_CSR(fcsr.fcsr.flags, &= ~RS1);
  else if(IS_INSN(CSRRW) && imm == CSR_FCSR) XRDR = DO_CSR(fcsr.bits, = XRS1);
  else if(IS_INSN(CSRRW) && imm == CSR_FRM) XRDR = DO_CSR(fcsr.fcsr.rm, = XRS1);
  else if(IS_INSN(CSRRW) && imm == CSR_FFLAGS) XRDR = DO_CSR(fcsr.fcsr.flags, = XRS1);
  else if(IS_INSN(CSRRWI) && imm == CSR_FCSR) XRDR = DO_CSR(fcsr.bits, = RS1);
  else if(IS_INSN(CSRRWI) && imm == CSR_FRM) XRDR = DO_CSR(fcsr.fcsr.rm, = RS1);
  else if(IS_INSN(CSRRWI) && imm == CSR_FFLAGS) XRDR = DO_CSR(fcsr.fcsr.flags, = RS1);
  else
    return -1;

  put_fcsr(fcsr);

  return 0;
}

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define PUT_FP_REG(which, type, val) asm("fmv." STR(type) ".x f" STR(which) ",%0" : : "r"(val))
#define GET_FP_REG(which, type, val) asm("fmv.x." STR(type) " %0,f" STR(which) : "=r"(val))
#define LOAD_FP_REG(which, type, val) asm("fl" STR(type) " f" STR(which) ",%0" : : "m"(val))
#define STORE_FP_REG(which, type, val)  asm("fs" STR(type) " f" STR(which) ",%0" : "=m"(val) : : "memory")

#endif

void fp_init()
{
  if (read_csr(mstatus) & MSTATUS_FS)
    for (int i = 0; i < 32; i++)
      put_f64_reg(i, 0);
}
