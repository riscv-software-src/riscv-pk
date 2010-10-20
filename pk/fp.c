#include "pcr.h"
#include "softfloat.h"
#include "riscv-opc.h"
#include "pk.h"
#include <stdint.h>

#define noisy 0

static void set_fp_reg(unsigned int which, unsigned int dp, uint64_t val);
static uint64_t get_fp_reg(unsigned int which, unsigned int dp);

static fp_state_t fp_state;
static void get_fp_state();
static void put_fp_state();

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
  fp_state.fsr = mfcr(CR_FSR);
  if(have_fp)
    get_fp_state();

  if(noisy)
    printk("FPU emulation at pc %lx, insn %x\n",tf->epc,(uint32_t)tf->insn);

  #define RRS1 ((tf->insn >> 15) & 0x1F)
  #define RRS2 ((tf->insn >> 20) & 0x1F)
  #define RRS3 ((tf->insn >>  5) & 0x1F)
  #define RRDR ( tf->insn        & 0x1F)
  #define RRDI RRS2

  #define IMM (((int32_t)tf->insn << 20) >> 20)

  #define XRS1 (tf->gpr[RRS1])
  #define XRS2 (tf->gpr[RRS2])
  #define XRDR (tf->gpr[RRDR])

  uint64_t frs1d = get_fp_reg(RRS1, 1);
  uint64_t frs2d = get_fp_reg(RRS2, 1);
  uint64_t frs3d = get_fp_reg(RRS3, 1);
  uint32_t frs1s = get_fp_reg(RRS1, 0);
  uint32_t frs2s = get_fp_reg(RRS2, 0);
  uint32_t frs3s = get_fp_reg(RRS3, 0);

  uint64_t effective_address = XRS1 + IMM;

  softfloat_exceptionFlags = 0;
  softfloat_roundingMode = (fp_state.fsr >> 5) & 3;

  #define IS_INSN(x) ((tf->insn & MASK_ ## x) == MATCH_ ## x)

  if(IS_INSN(L_S))
  {
    validate_address(tf, effective_address, 4, 0);
    set_fp_reg(RRDI, 0, *(uint32_t*)effective_address);
  }
  else if(IS_INSN(L_D))
  {
    validate_address(tf, effective_address, 8, 0);
    set_fp_reg(RRDI, 1, *(uint64_t*)effective_address);
  }
  else if(IS_INSN(S_S))
  {
    validate_address(tf, effective_address, 4, 1);
    *(uint32_t*)effective_address = frs2s;
  }
  else if(IS_INSN(S_D))
  {
    validate_address(tf, effective_address, 8, 1);
    *(uint64_t*)effective_address = frs2d;
  }
  else if(IS_INSN(MFF_S))
    XRDR = frs2s;
  else if(IS_INSN(MFF_D))
    XRDR = frs2d;
  else if(IS_INSN(MFFL_D))
    XRDR = (int32_t)frs2d;
  else if(IS_INSN(MFFH_D))
    XRDR = (int64_t)frs2d >> 32;
  else if(IS_INSN(MTF_S))
    set_fp_reg(RRDR, 0, XRS1);
  else if(IS_INSN(MTF_D))
    set_fp_reg(RRDR, 1, XRS1);
  else if(IS_INSN(MTFLH_D))
    set_fp_reg(RRDR, 1, (uint32_t)XRS1 | (XRS2 << 32));
  else if(IS_INSN(SGNINJ_S))
    set_fp_reg(RRDR, 0, (frs1s &~ (uint32_t)INT32_MIN) | (frs2s & (uint32_t)INT32_MIN));
  else if(IS_INSN(SGNINJ_D))
    set_fp_reg(RRDR, 1, (frs1d &~ INT64_MIN) | (frs2d & INT64_MIN));
  else if(IS_INSN(SGNINJN_S))
    set_fp_reg(RRDR, 0, (frs1s &~ (uint32_t)INT32_MIN) | ((~frs2s) & (uint32_t)INT32_MIN));
  else if(IS_INSN(SGNINJN_D))
    set_fp_reg(RRDR, 1, (frs1d &~ INT64_MIN) | ((~frs2d) & INT64_MIN));
  else if(IS_INSN(SGNMUL_S))
    set_fp_reg(RRDR, 0, frs1s ^ (frs2s & (uint32_t)INT32_MIN));
  else if(IS_INSN(SGNMUL_D))
    set_fp_reg(RRDR, 1, frs1d ^ (frs2d & INT64_MIN));
  else if(IS_INSN(C_EQ_S))
    XRDR = f32_eq(frs1s, frs2s);
  else if(IS_INSN(C_EQ_D))
    XRDR = f64_eq(frs1d, frs2d);
  else if(IS_INSN(C_LE_S))
    XRDR = f32_le(frs1s, frs2s);
  else if(IS_INSN(C_LE_D))
    XRDR = f64_le(frs1d, frs2d);
  else if(IS_INSN(C_LT_S))
    XRDR = f32_lt(frs1s, frs2s);
  else if(IS_INSN(C_LT_D))
    XRDR = f64_lt(frs1d, frs2d);
  else if(IS_INSN(CVT_S_W))
    set_fp_reg(RRDR, 0, i32_to_f32(XRS1));
  else if(IS_INSN(CVT_S_L))
    set_fp_reg(RRDR, 0, i64_to_f32(XRS1));
  else if(IS_INSN(CVT_S_D))
    set_fp_reg(RRDR, 0, f64_to_f32(frs1d));
  else if(IS_INSN(CVT_D_W))
    set_fp_reg(RRDR, 1, i32_to_f64(XRS1));
  else if(IS_INSN(CVT_D_L))
    set_fp_reg(RRDR, 1, i64_to_f64(XRS1));
  else if(IS_INSN(CVT_D_S))
    set_fp_reg(RRDR, 1, f32_to_f64(frs1s));
  else if(IS_INSN(CVTU_S_W))
    set_fp_reg(RRDR, 0, ui32_to_f32(XRS1));
  else if(IS_INSN(CVTU_S_L))
    set_fp_reg(RRDR, 0, ui64_to_f32(XRS1));
  else if(IS_INSN(CVTU_D_W))
    set_fp_reg(RRDR, 1, ui32_to_f64(XRS1));
  else if(IS_INSN(CVTU_D_L))
    set_fp_reg(RRDR, 1, ui64_to_f64(XRS1));
  else if(IS_INSN(ADD_S))
    set_fp_reg(RRDR, 0, f32_add(frs1s, frs2s));
  else if(IS_INSN(ADD_D))
    set_fp_reg(RRDR, 1, f64_add(frs1d, frs2d));
  else if(IS_INSN(SUB_S))
    set_fp_reg(RRDR, 0, f32_sub(frs1s, frs2s));
  else if(IS_INSN(SUB_D))
    set_fp_reg(RRDR, 1, f64_sub(frs1d, frs2d));
  else if(IS_INSN(MUL_S))
    set_fp_reg(RRDR, 0, f32_mul(frs1s, frs2s));
  else if(IS_INSN(MUL_D))
    set_fp_reg(RRDR, 1, f64_mul(frs1d, frs2d));
  else if(IS_INSN(MADD_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(frs1s, frs2s, frs3s));
  else if(IS_INSN(MADD_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(frs1d, frs2d, frs3d));
  else if(IS_INSN(MSUB_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(frs1s, frs2s, frs3s ^ (uint32_t)INT32_MIN));
  else if(IS_INSN(MSUB_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(frs1d, frs2d, frs3d ^ INT64_MIN));
  else if(IS_INSN(NMADD_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(frs1s, frs2s, frs3s) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(NMADD_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(frs1d, frs2d, frs3d) ^ INT64_MIN);
  else if(IS_INSN(NMSUB_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(frs1s, frs2s, frs3s ^ (uint32_t)INT32_MIN) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(NMSUB_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(frs1d, frs2d, frs3d ^ INT64_MIN) ^ INT64_MIN);
  else if(IS_INSN(DIV_S))
    set_fp_reg(RRDR, 0, f32_div(frs1s, frs2s));
  else if(IS_INSN(DIV_D))
    set_fp_reg(RRDR, 1, f64_div(frs1d, frs2d));
  else if(IS_INSN(SQRT_S))
    set_fp_reg(RRDR, 0, f32_sqrt(frs1s));
  else if(IS_INSN(SQRT_D))
    set_fp_reg(RRDR, 1, f64_sqrt(frs1d));
  else if(IS_INSN(TRUNC_W_S))
    XRDR = f32_to_i32_r_minMag(frs1s,true);
  else if(IS_INSN(TRUNC_W_D))
    XRDR = f64_to_i32_r_minMag(frs1d,true);
  else if(IS_INSN(TRUNC_L_S))
    XRDR = f32_to_i64_r_minMag(frs1s,true);
  else if(IS_INSN(TRUNC_L_D))
    XRDR = f64_to_i64_r_minMag(frs1d,true);
  else if(IS_INSN(TRUNCU_W_S))
    XRDR = f32_to_ui32_r_minMag(frs1s,true);
  else if(IS_INSN(TRUNCU_W_D))
    XRDR = f64_to_ui32_r_minMag(frs1d,true);
  else if(IS_INSN(TRUNCU_L_S))
    XRDR = f32_to_ui64_r_minMag(frs1s,true);
  else if(IS_INSN(TRUNCU_L_D))
    XRDR = f64_to_ui64_r_minMag(frs1d,true);
  else
    return -1;

  mtcr(fp_state.fsr, CR_FSR);
  if(have_fp)
    put_fp_state();

  advance_pc(tf);

  return 0;
}

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define PUT_FP_REG(which, type, val) asm("mtf." STR(type) " $f" STR(which) ",%0" : : "r"(val))
#define GET_FP_REG(which, type, val) asm("mff." STR(type) " %0,$f" STR(which) : "=r"(val))

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
    uint64_t tmp;
    GET_FP_REG(0,d,tmp);
    PUT_FP_REG(0,s,val);
    GET_FP_REG(0,d,fp_state.fpr[which]);
    PUT_FP_REG(0,d,tmp);
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
    uint64_t tmp;
    GET_FP_REG(0,d,tmp);
    PUT_FP_REG(0,d,fp_state.fpr[which]);
    GET_FP_REG(0,s,val);
    PUT_FP_REG(0,d,tmp);
  }

  if(noisy)
  {
    printk("fpr%c[%x] => ",dp?'d':'s',which);
    printk("%lx\n",val);
  }

  return val;
}

static void __attribute__((noinline)) get_fp_state()
{
  GET_FP_REG(0, d, fp_state.fpr[0]);
  GET_FP_REG(1, d, fp_state.fpr[1]);
  GET_FP_REG(2, d, fp_state.fpr[2]);
  GET_FP_REG(3, d, fp_state.fpr[3]);
  GET_FP_REG(4, d, fp_state.fpr[4]);
  GET_FP_REG(5, d, fp_state.fpr[5]);
  GET_FP_REG(6, d, fp_state.fpr[6]);
  GET_FP_REG(7, d, fp_state.fpr[7]);
  GET_FP_REG(8, d, fp_state.fpr[8]);
  GET_FP_REG(9, d, fp_state.fpr[9]);
  GET_FP_REG(10, d, fp_state.fpr[10]);
  GET_FP_REG(11, d, fp_state.fpr[11]);
  GET_FP_REG(12, d, fp_state.fpr[12]);
  GET_FP_REG(13, d, fp_state.fpr[13]);
  GET_FP_REG(14, d, fp_state.fpr[14]);
  GET_FP_REG(15, d, fp_state.fpr[15]);
  GET_FP_REG(16, d, fp_state.fpr[16]);
  GET_FP_REG(17, d, fp_state.fpr[17]);
  GET_FP_REG(18, d, fp_state.fpr[18]);
  GET_FP_REG(19, d, fp_state.fpr[19]);
  GET_FP_REG(20, d, fp_state.fpr[20]);
  GET_FP_REG(21, d, fp_state.fpr[21]);
  GET_FP_REG(22, d, fp_state.fpr[22]);
  GET_FP_REG(23, d, fp_state.fpr[23]);
  GET_FP_REG(24, d, fp_state.fpr[24]);
  GET_FP_REG(25, d, fp_state.fpr[25]);
  GET_FP_REG(26, d, fp_state.fpr[26]);
  GET_FP_REG(27, d, fp_state.fpr[27]);
  GET_FP_REG(28, d, fp_state.fpr[28]);
  GET_FP_REG(29, d, fp_state.fpr[29]);
  GET_FP_REG(30, d, fp_state.fpr[30]);
  GET_FP_REG(31, d, fp_state.fpr[31]);
}

static void __attribute__((noinline)) put_fp_state()
{
  PUT_FP_REG(0, d, fp_state.fpr[0]);
  PUT_FP_REG(1, d, fp_state.fpr[1]);
  PUT_FP_REG(2, d, fp_state.fpr[2]);
  PUT_FP_REG(3, d, fp_state.fpr[3]);
  PUT_FP_REG(4, d, fp_state.fpr[4]);
  PUT_FP_REG(5, d, fp_state.fpr[5]);
  PUT_FP_REG(6, d, fp_state.fpr[6]);
  PUT_FP_REG(7, d, fp_state.fpr[7]);
  PUT_FP_REG(8, d, fp_state.fpr[8]);
  PUT_FP_REG(9, d, fp_state.fpr[9]);
  PUT_FP_REG(10, d, fp_state.fpr[10]);
  PUT_FP_REG(11, d, fp_state.fpr[11]);
  PUT_FP_REG(12, d, fp_state.fpr[12]);
  PUT_FP_REG(13, d, fp_state.fpr[13]);
  PUT_FP_REG(14, d, fp_state.fpr[14]);
  PUT_FP_REG(15, d, fp_state.fpr[15]);
  PUT_FP_REG(16, d, fp_state.fpr[16]);
  PUT_FP_REG(17, d, fp_state.fpr[17]);
  PUT_FP_REG(18, d, fp_state.fpr[18]);
  PUT_FP_REG(19, d, fp_state.fpr[19]);
  PUT_FP_REG(20, d, fp_state.fpr[20]);
  PUT_FP_REG(21, d, fp_state.fpr[21]);
  PUT_FP_REG(22, d, fp_state.fpr[22]);
  PUT_FP_REG(23, d, fp_state.fpr[23]);
  PUT_FP_REG(24, d, fp_state.fpr[24]);
  PUT_FP_REG(25, d, fp_state.fpr[25]);
  PUT_FP_REG(26, d, fp_state.fpr[26]);
  PUT_FP_REG(27, d, fp_state.fpr[27]);
  PUT_FP_REG(28, d, fp_state.fpr[28]);
  PUT_FP_REG(29, d, fp_state.fpr[29]);
  PUT_FP_REG(30, d, fp_state.fpr[30]);
  PUT_FP_REG(31, d, fp_state.fpr[31]);
}

void init_fp_regs()
{
  long sr = mfpcr(PCR_SR);
  mtpcr(sr | SR_EF, PCR_SR);
  put_fp_state();
  mtpcr(sr, PCR_SR);
}

