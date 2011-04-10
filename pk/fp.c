#include "pk.h"
#include "pcr.h"
#include "fp.h"

static fp_state_t fp_state;

#ifdef PK_ENABLE_FP_EMULATION

#include "softfloat.h"
#include "riscv-opc.h"
#include <stdint.h>

#define noisy 0

static void set_fp_reg(unsigned int which, unsigned int dp, uint64_t val);
static uint64_t get_fp_reg(unsigned int which, unsigned int dp);

static inline void
validate_address(trapframe_t* tf, long addr, int size, int store)
{
  if(addr & (size-1))
    handle_misaligned_ldst(tf);
  if(addr >= USER_MEM_SIZE)
    store ? handle_fault_store(tf) : handle_fault_load(tf);
}

int emulate_fp(trapframe_t* tf)
{
  if(have_fp)
  {
    if(!(mfpcr(PCR_SR) & SR_EF))
      init_fp(tf);
    fp_state.fsr = get_fp_state(fp_state.fpr);
  }

  if(noisy)
    printk("FPU emulation at pc %lx, insn %x\n",tf->epc,(uint32_t)tf->insn);

  #define RRS1 ((tf->insn >> 22) & 0x1F)
  #define RRS2 ((tf->insn >> 17) & 0x1F)
  #define RRS3 ((tf->insn >> 12) & 0x1F)
  #define RRD  ((tf->insn >> 27) & 0x1F)
  #define RM   ((tf->insn >>  9) &  0x3) 

  int32_t imm = ((int32_t)tf->insn << 10) >> 20;
  int32_t bimm = (((tf->insn >> 27) & 0x1f) << 7) | ((tf->insn >> 10) & 0x7f);
  bimm = (bimm << 20) >> 20;

  #define XRS1 (tf->gpr[RRS1])
  #define XRS2 (tf->gpr[RRS2])
  #define XRDR (tf->gpr[RRD])

  uint64_t frs1d = fp_state.fpr[RRS1];
  uint64_t frs2d = fp_state.fpr[RRS2];
  uint64_t frs3d = fp_state.fpr[RRS3];
  uint32_t frs1s = get_fp_reg(RRS1, 0);
  uint32_t frs2s = get_fp_reg(RRS2, 0);
  uint32_t frs3s = get_fp_reg(RRS3, 0);

  long effective_address_load = XRS1 + imm;
  long effective_address_store = XRS1 + bimm;

  softfloat_exceptionFlags = 0;
  softfloat_roundingMode = (RM & 4) ? (RM & 3) : ((fp_state.fsr >> 5) & 3);

  #define IS_INSN(x) ((tf->insn & MASK_ ## x) == MATCH_ ## x)

  if(IS_INSN(FLW))
  {
    validate_address(tf, effective_address_load, 4, 0);
    set_fp_reg(RRD, 0, *(uint32_t*)effective_address_load);
  }
  else if(IS_INSN(FLD))
  {
    validate_address(tf, effective_address_load, 8, 0);
    set_fp_reg(RRD, 1, *(uint64_t*)effective_address_load);
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
  else if(IS_INSN(MFTX_S))
    XRDR = frs2s;
  else if(IS_INSN(MFTX_D))
    XRDR = frs2d;
  else if(IS_INSN(MXTF_S))
    set_fp_reg(RRD, 0, XRS1);
  else if(IS_INSN(MXTF_D))
    set_fp_reg(RRD, 1, XRS1);
  else if(IS_INSN(FSGNJ_S))
    set_fp_reg(RRD, 0, (frs1s &~ (uint32_t)INT32_MIN) | (frs2s & (uint32_t)INT32_MIN));
  else if(IS_INSN(FSGNJ_D))
    set_fp_reg(RRD, 1, (frs1d &~ INT64_MIN) | (frs2d & INT64_MIN));
  else if(IS_INSN(FSGNJN_S))
    set_fp_reg(RRD, 0, (frs1s &~ (uint32_t)INT32_MIN) | ((~frs2s) & (uint32_t)INT32_MIN));
  else if(IS_INSN(FSGNJN_D))
    set_fp_reg(RRD, 1, (frs1d &~ INT64_MIN) | ((~frs2d) & INT64_MIN));
  else if(IS_INSN(FSGNJX_S))
    set_fp_reg(RRD, 0, frs1s ^ (frs2s & (uint32_t)INT32_MIN));
  else if(IS_INSN(FSGNJX_D))
    set_fp_reg(RRD, 1, frs1d ^ (frs2d & INT64_MIN));
  else if(IS_INSN(FEQ_S))
    XRDR = f32_eq(frs1s, frs2s);
  else if(IS_INSN(FEQ_D))
    XRDR = f64_eq(frs1d, frs2d);
  else if(IS_INSN(FLE_S))
    XRDR = f32_le(frs1s, frs2s);
  else if(IS_INSN(FLE_D))
    XRDR = f64_le(frs1d, frs2d);
  else if(IS_INSN(FLT_S))
    XRDR = f32_lt(frs1s, frs2s);
  else if(IS_INSN(FLT_D))
    XRDR = f64_lt(frs1d, frs2d);
  else if(IS_INSN(FCVT_S_W))
    set_fp_reg(RRD, 0, i32_to_f32(XRS1));
  else if(IS_INSN(FCVT_S_L))
    set_fp_reg(RRD, 0, i64_to_f32(XRS1));
  else if(IS_INSN(FCVT_S_D))
    set_fp_reg(RRD, 0, f64_to_f32(frs1d));
  else if(IS_INSN(FCVT_D_W))
    set_fp_reg(RRD, 1, i32_to_f64(XRS1));
  else if(IS_INSN(FCVT_D_L))
    set_fp_reg(RRD, 1, i64_to_f64(XRS1));
  else if(IS_INSN(FCVT_D_S))
    set_fp_reg(RRD, 1, f32_to_f64(frs1s));
  else if(IS_INSN(FCVT_S_WU))
    set_fp_reg(RRD, 0, ui32_to_f32(XRS1));
  else if(IS_INSN(FCVT_S_LU))
    set_fp_reg(RRD, 0, ui64_to_f32(XRS1));
  else if(IS_INSN(FCVT_D_WU))
    set_fp_reg(RRD, 1, ui32_to_f64(XRS1));
  else if(IS_INSN(FCVT_D_LU))
    set_fp_reg(RRD, 1, ui64_to_f64(XRS1));
  else if(IS_INSN(FADD_S))
    set_fp_reg(RRD, 0, f32_add(frs1s, frs2s));
  else if(IS_INSN(FADD_D))
    set_fp_reg(RRD, 1, f64_add(frs1d, frs2d));
  else if(IS_INSN(FSUB_S))
    set_fp_reg(RRD, 0, f32_sub(frs1s, frs2s));
  else if(IS_INSN(FSUB_D))
    set_fp_reg(RRD, 1, f64_sub(frs1d, frs2d));
  else if(IS_INSN(FMUL_S))
    set_fp_reg(RRD, 0, f32_mul(frs1s, frs2s));
  else if(IS_INSN(FMUL_D))
    set_fp_reg(RRD, 1, f64_mul(frs1d, frs2d));
  else if(IS_INSN(FMADD_S))
    set_fp_reg(RRD, 0, f32_mulAdd(frs1s, frs2s, frs3s));
  else if(IS_INSN(FMADD_D))
    set_fp_reg(RRD, 1, f64_mulAdd(frs1d, frs2d, frs3d));
  else if(IS_INSN(FMSUB_S))
    set_fp_reg(RRD, 0, f32_mulAdd(frs1s, frs2s, frs3s ^ (uint32_t)INT32_MIN));
  else if(IS_INSN(FMSUB_D))
    set_fp_reg(RRD, 1, f64_mulAdd(frs1d, frs2d, frs3d ^ INT64_MIN));
  else if(IS_INSN(FNMADD_S))
    set_fp_reg(RRD, 0, f32_mulAdd(frs1s, frs2s, frs3s) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(FNMADD_D))
    set_fp_reg(RRD, 1, f64_mulAdd(frs1d, frs2d, frs3d) ^ INT64_MIN);
  else if(IS_INSN(FNMSUB_S))
    set_fp_reg(RRD, 0, f32_mulAdd(frs1s, frs2s, frs3s ^ (uint32_t)INT32_MIN) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(FNMSUB_D))
    set_fp_reg(RRD, 1, f64_mulAdd(frs1d, frs2d, frs3d ^ INT64_MIN) ^ INT64_MIN);
  else if(IS_INSN(FDIV_S))
    set_fp_reg(RRD, 0, f32_div(frs1s, frs2s));
  else if(IS_INSN(FDIV_D))
    set_fp_reg(RRD, 1, f64_div(frs1d, frs2d));
  else if(IS_INSN(FSQRT_S))
    set_fp_reg(RRD, 0, f32_sqrt(frs1s));
  else if(IS_INSN(FSQRT_D))
    set_fp_reg(RRD, 1, f64_sqrt(frs1d));
  else if(IS_INSN(FCVT_W_S))
    XRDR = f32_to_i32_r_minMag(frs1s,true);
  else if(IS_INSN(FCVT_W_D))
    XRDR = f64_to_i32_r_minMag(frs1d,true);
  else if(IS_INSN(FCVT_L_S))
    XRDR = f32_to_i64_r_minMag(frs1s,true);
  else if(IS_INSN(FCVT_L_D))
    XRDR = f64_to_i64_r_minMag(frs1d,true);
  else if(IS_INSN(FCVT_WU_S))
    XRDR = f32_to_ui32_r_minMag(frs1s,true);
  else if(IS_INSN(FCVT_WU_D))
    XRDR = f64_to_ui32_r_minMag(frs1d,true);
  else if(IS_INSN(FCVT_LU_S))
    XRDR = f32_to_ui64_r_minMag(frs1s,true);
  else if(IS_INSN(FCVT_LU_D))
    XRDR = f64_to_ui64_r_minMag(frs1d,true);
  else
    return -1;

  if(have_fp)
    put_fp_state(fp_state.fpr,fp_state.fsr);

  advance_pc(tf);

  return 0;
}

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define PUT_FP_REG(which, type, val) asm("mxtf." STR(type) " $f" STR(which) ",%0" : : "r"(val))
#define GET_FP_REG(which, type, val) asm("mftx." STR(type) " %0,$f" STR(which) : "=r"(val))
#define LOAD_FP_REG(which, type, val) asm("fl" STR(type) " $f" STR(which) ",%0" : : "m"(val))
#define STORE_FP_REG(which, type, val)  asm("fs" STR(type) " $f" STR(which) ",%0" : "=m"(val) : : "memory")

static void __attribute__((noinline))
set_fp_reg(unsigned int which, unsigned int dp, uint64_t val)
{
  if(noisy)
  {
    printk("fpr%c[%x] <= ",dp?'d':'s',which);
    printk("%lx\n",val);
  }

  if(dp || !have_fp)
    fp_state.fpr[which] = val;
  else
  {
    // to set an SP value, move the SP value into the FPU
    // then move it back out as a DP value.  OK to clobber $f0
    // because we'll restore it later.
    PUT_FP_REG(0,s,val);
    STORE_FP_REG(0,d,fp_state.fpr[which]);
  }
}

static uint64_t __attribute__((noinline))
get_fp_reg(unsigned int which, unsigned int dp)
{
  uint64_t val;
  if(dp || !have_fp)
    val = fp_state.fpr[which];
  else
  {
    // to get an SP value, move the DP value into the FPU
    // then move it back out as an SP value.  OK to clobber $f0
    // because we'll restore it later.
    LOAD_FP_REG(0,d,fp_state.fpr[which]);
    GET_FP_REG(0,s,val);
  }

  if(noisy)
  {
    printk("fpr%c[%x] => ",dp?'d':'s',which);
    printk("%lx\n",val);
  }

  return val;
}

#endif

void init_fp(trapframe_t* tf)
{
  long sr = mfpcr(PCR_SR);
  mtpcr(sr | SR_EF, PCR_SR);

  put_fp_state(fp_state.fpr,fp_state.fsr);

  tf->sr |= SR_EF;
}
