#include "softfloat.h"
#include "riscv-opc.h"
#include "pk.h"
#include <stdint.h>

#define noisy 0

static void set_fp_reg(unsigned int which, unsigned int dp, uint64_t val);
static uint64_t get_fp_reg(unsigned int which, unsigned int dp);

uint64_t fp_regs[32];
uint32_t fsr;

void validate_address(trapframe_t* tf, long addr, int size, int store)
{
  if(addr & (size-1))
    handle_misaligned_ldst(tf);
  if(addr >= USER_MEM_SIZE)
    store ? handle_fault_store(tf) : handle_fault_load(tf);
}

int emulate_fp(trapframe_t* tf)
{
  if(noisy)
    printk("FPU emulation at pc %lx, insn %x\n",tf->epc,(uint32_t)tf->insn);

  #define RRS1 ((tf->insn >> 15) & 0x1F)
  #define RRS2 ((tf->insn >> 20) & 0x1F)
  #define RRS3 ((tf->insn >>  5) & 0x1F)
  #define RRDR ( tf->insn        & 0x1F)
  #define RRDI RRS2

  #define XRS1 (tf->gpr[RRS1])
  #define XRS2 (tf->gpr[RRS2])
  #define XRDR (tf->gpr[RRDR])
  #define FRS1S get_fp_reg(RRS1,0)
  #define FRS2S get_fp_reg(RRS2,0)
  #define FRS3S get_fp_reg(RRS3,0)
  #define FRS1D get_fp_reg(RRS1,1)
  #define FRS2D get_fp_reg(RRS2,1)
  #define FRS3D get_fp_reg(RRS3,1)

  #define IMM (((int32_t)tf->insn << 20) >> 20)
  #define EFFECTIVE_ADDRESS (XRS1+IMM)

  #define IS_INSN(x) ((tf->insn & MASK_ ## x) == MATCH_ ## x)

  if(IS_INSN(L_S))
  {
    validate_address(tf, EFFECTIVE_ADDRESS, 4, 0);
    set_fp_reg(RRDI, 0, *(uint32_t*)EFFECTIVE_ADDRESS);
  }
  else if(IS_INSN(L_D))
  {
    validate_address(tf, EFFECTIVE_ADDRESS, 8, 0);
    set_fp_reg(RRDI, 1, *(uint64_t*)EFFECTIVE_ADDRESS);
  }
  else if(IS_INSN(S_S))
  {
    validate_address(tf, EFFECTIVE_ADDRESS, 4, 1);
    *(uint32_t*)EFFECTIVE_ADDRESS = FRS2S;
  }
  else if(IS_INSN(S_D))
  {
    validate_address(tf, EFFECTIVE_ADDRESS, 8, 1);
    *(uint64_t*)EFFECTIVE_ADDRESS = FRS2D;
  }
  else if(IS_INSN(MFF_S))
    XRDR = FRS2S;
  else if(IS_INSN(MFF_D))
    XRDR = FRS2D;
  else if(IS_INSN(MFFL_D))
    XRDR = (int32_t)FRS2D;
  else if(IS_INSN(MFFH_D))
    XRDR = (int64_t)FRS2D >> 32;
  else if(IS_INSN(MTF_S))
    set_fp_reg(RRDR, 0, XRS1);
  else if(IS_INSN(MTF_D))
    set_fp_reg(RRDR, 1, XRS1);
  else if(IS_INSN(MTFLH_D))
    set_fp_reg(RRDR, 1, (uint32_t)XRS1 | (XRS2 << 32));
  else if(IS_INSN(SGNINJ_S))
    set_fp_reg(RRDR, 0, (FRS1S &~ (uint32_t)INT32_MIN) | (FRS2S & (uint32_t)INT32_MIN));
  else if(IS_INSN(SGNINJ_D))
    set_fp_reg(RRDR, 1, (FRS1D &~ INT64_MIN) | (FRS2D & INT64_MIN));
  else if(IS_INSN(SGNINJN_S))
    set_fp_reg(RRDR, 0, (FRS1S &~ (uint32_t)INT32_MIN) | ((~FRS2S) & (uint32_t)INT32_MIN));
  else if(IS_INSN(SGNINJN_D))
    set_fp_reg(RRDR, 1, (FRS1D &~ INT64_MIN) | ((~FRS2D) & INT64_MIN));
  else if(IS_INSN(SGNMUL_S))
    set_fp_reg(RRDR, 0, FRS1S ^ (FRS2S & (uint32_t)INT32_MIN));
  else if(IS_INSN(SGNMUL_D))
    set_fp_reg(RRDR, 1, FRS1D ^ (FRS2D & INT64_MIN));
  else if(IS_INSN(C_EQ_S))
    XRDR = f32_eq(FRS1S, FRS2S);
  else if(IS_INSN(C_EQ_D))
    XRDR = f64_eq(FRS1D, FRS2D);
  else if(IS_INSN(C_LE_S))
    XRDR = f32_le(FRS1S, FRS2S);
  else if(IS_INSN(C_LE_D))
    XRDR = f64_le(FRS1D, FRS2D);
  else if(IS_INSN(C_LT_S))
    XRDR = f32_lt(FRS1S, FRS2S);
  else if(IS_INSN(C_LT_D))
    XRDR = f64_lt(FRS1D, FRS2D);
  else if(IS_INSN(CVT_S_W))
    set_fp_reg(RRDR, 0, i32_to_f32(XRS1));
  else if(IS_INSN(CVT_S_L))
    set_fp_reg(RRDR, 0, i64_to_f32(XRS1));
  else if(IS_INSN(CVT_S_D))
    set_fp_reg(RRDR, 0, f64_to_f32(FRS1D));
  else if(IS_INSN(CVT_D_W))
    set_fp_reg(RRDR, 1, i32_to_f64(XRS1));
  else if(IS_INSN(CVT_D_L))
    set_fp_reg(RRDR, 1, i64_to_f64(XRS1));
  else if(IS_INSN(CVT_D_S))
    set_fp_reg(RRDR, 1, f32_to_f64(FRS1S));
  else if(IS_INSN(CVTU_S_W))
    set_fp_reg(RRDR, 0, ui32_to_f32(XRS1));
  else if(IS_INSN(CVTU_S_L))
    set_fp_reg(RRDR, 0, ui64_to_f32(XRS1));
  else if(IS_INSN(CVTU_D_W))
    set_fp_reg(RRDR, 1, ui32_to_f64(XRS1));
  else if(IS_INSN(CVTU_D_L))
    set_fp_reg(RRDR, 1, ui64_to_f64(XRS1));
  else if(IS_INSN(ADD_S))
    set_fp_reg(RRDR, 0, f32_add(FRS1S, FRS2S));
  else if(IS_INSN(ADD_D))
    set_fp_reg(RRDR, 1, f64_add(FRS1D, FRS2D));
  else if(IS_INSN(SUB_S))
    set_fp_reg(RRDR, 0, f32_sub(FRS1S, FRS2S));
  else if(IS_INSN(SUB_D))
    set_fp_reg(RRDR, 1, f64_sub(FRS1D, FRS2D));
  else if(IS_INSN(MUL_S))
    set_fp_reg(RRDR, 0, f32_mul(FRS1S, FRS2S));
  else if(IS_INSN(MUL_D))
    set_fp_reg(RRDR, 1, f64_mul(FRS1D, FRS2D));
  else if(IS_INSN(MADD_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(FRS1S, FRS2S, FRS3S));
  else if(IS_INSN(MADD_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(FRS1D, FRS2D, FRS3D));
  else if(IS_INSN(MSUB_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(FRS1S, FRS2S, FRS3S ^ (uint32_t)INT32_MIN));
  else if(IS_INSN(MSUB_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(FRS1D, FRS2D, FRS3D ^ INT64_MIN));
  else if(IS_INSN(NMADD_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(FRS1S, FRS2S, FRS3S) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(NMADD_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(FRS1D, FRS2D, FRS3D) ^ INT64_MIN);
  else if(IS_INSN(NMSUB_S))
    set_fp_reg(RRDR, 0, f32_mulAdd(FRS1S, FRS2S, FRS3S ^ (uint32_t)INT32_MIN) ^ (uint32_t)INT32_MIN);
  else if(IS_INSN(NMSUB_D))
    set_fp_reg(RRDR, 1, f64_mulAdd(FRS1D, FRS2D, FRS3D ^ INT64_MIN) ^ INT64_MIN);
  else if(IS_INSN(DIV_S))
    set_fp_reg(RRDR, 0, f32_div(FRS1S, FRS2S));
  else if(IS_INSN(DIV_D))
    set_fp_reg(RRDR, 1, f64_div(FRS1D, FRS2D));
  else if(IS_INSN(SQRT_S))
    set_fp_reg(RRDR, 0, f32_sqrt(FRS1S));
  else if(IS_INSN(SQRT_D))
    set_fp_reg(RRDR, 1, f64_sqrt(FRS1D));
  else if(IS_INSN(TRUNC_W_S))
    XRDR = f32_to_i32_r_minMag(FRS1S,true);
  else if(IS_INSN(TRUNC_W_D))
    XRDR = f64_to_i32_r_minMag(FRS1D,true);
  else if(IS_INSN(TRUNC_L_S))
    XRDR = f32_to_i64_r_minMag(FRS1S,true);
  else if(IS_INSN(TRUNC_L_D))
    XRDR = f64_to_i64_r_minMag(FRS1D,true);
  else if(IS_INSN(TRUNCU_W_S))
    XRDR = f32_to_ui32_r_minMag(FRS1S,true);
  else if(IS_INSN(TRUNCU_W_D))
    XRDR = f64_to_ui32_r_minMag(FRS1D,true);
  else if(IS_INSN(TRUNCU_L_S))
    XRDR = f32_to_ui64_r_minMag(FRS1S,true);
  else if(IS_INSN(TRUNCU_L_D))
    XRDR = f64_to_ui64_r_minMag(FRS1D,true);
  else
    return -1;

  advance_pc(tf);

  return 0;
}

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define PUT_FP_REG_S(which) case which: \
                if(have_fp) \
                  asm volatile("mtf.s $f" STR(which) ",%0" : : "r"(val)); \
                else fp_regs[which] = val; \
                if(noisy) printk("set fp sp reg %x to %x\n",which,val); \
                break
#define PUT_FP_REG(which, val) asm volatile("mtf.d $f" STR(which) ",%0" : : "r"(val))
#define PUT_FP_REG_D(which) case 32+which: \
                if(have_fp) \
                  PUT_FP_REG(which,val); \
                else fp_regs[which] = val; \
                if(noisy) printk("set fp dp reg %x to %x%x\n",which,(uint32_t)(val>>32),(uint32_t)val); \
                break
#define GET_FP_REG_S(which) case which: \
                if(have_fp) asm volatile("mff.s %0,$f" STR(which) : "=r"(val));\
                else val = (uint64_t)(int64_t)(int32_t)fp_regs[which]; \
                if(noisy) printk("get fp sp reg %x  v %x\n",which,val); \
                break
#define GET_FP_REG_D(which) case 32+which: \
                if(have_fp) asm volatile("mff.d %0,$f" STR(which) : "=r"(val));\
                else val = fp_regs[which]; \
                if(noisy) printk("get fp dp reg %x  v %x%x\n",which,(uint32_t)(val>>32),(uint32_t)val); \
                break

static void set_fp_reg(unsigned int which, unsigned int dp, uint64_t val)
{
  switch(which + (!!dp)*32)
  {
    PUT_FP_REG_S(0);
    PUT_FP_REG_S(1);
    PUT_FP_REG_S(2);
    PUT_FP_REG_S(3);
    PUT_FP_REG_S(4);
    PUT_FP_REG_S(5);
    PUT_FP_REG_S(6);
    PUT_FP_REG_S(7);
    PUT_FP_REG_S(8);
    PUT_FP_REG_S(9);
    PUT_FP_REG_S(10);
    PUT_FP_REG_S(11);
    PUT_FP_REG_S(12);
    PUT_FP_REG_S(13);
    PUT_FP_REG_S(14);
    PUT_FP_REG_S(15);
    PUT_FP_REG_S(16);
    PUT_FP_REG_S(17);
    PUT_FP_REG_S(18);
    PUT_FP_REG_S(19);
    PUT_FP_REG_S(20);
    PUT_FP_REG_S(21);
    PUT_FP_REG_S(22);
    PUT_FP_REG_S(23);
    PUT_FP_REG_S(24);
    PUT_FP_REG_S(25);
    PUT_FP_REG_S(26);
    PUT_FP_REG_S(27);
    PUT_FP_REG_S(28);
    PUT_FP_REG_S(29);
    PUT_FP_REG_S(30);
    PUT_FP_REG_S(31);
    PUT_FP_REG_D(0);
    PUT_FP_REG_D(1);
    PUT_FP_REG_D(2);
    PUT_FP_REG_D(3);
    PUT_FP_REG_D(4);
    PUT_FP_REG_D(5);
    PUT_FP_REG_D(6);
    PUT_FP_REG_D(7);
    PUT_FP_REG_D(8);
    PUT_FP_REG_D(9);
    PUT_FP_REG_D(10);
    PUT_FP_REG_D(11);
    PUT_FP_REG_D(12);
    PUT_FP_REG_D(13);
    PUT_FP_REG_D(14);
    PUT_FP_REG_D(15);
    PUT_FP_REG_D(16);
    PUT_FP_REG_D(17);
    PUT_FP_REG_D(18);
    PUT_FP_REG_D(19);
    PUT_FP_REG_D(20);
    PUT_FP_REG_D(21);
    PUT_FP_REG_D(22);
    PUT_FP_REG_D(23);
    PUT_FP_REG_D(24);
    PUT_FP_REG_D(25);
    PUT_FP_REG_D(26);
    PUT_FP_REG_D(27);
    PUT_FP_REG_D(28);
    PUT_FP_REG_D(29);
    PUT_FP_REG_D(30);
    PUT_FP_REG_D(31);
    default:
      panic("bad fp register");
  }
}

static uint64_t get_fp_reg(unsigned int which, unsigned int dp)
{
  uint64_t val;
  switch(which + (!!dp)*32)
  {
    GET_FP_REG_S(0);
    GET_FP_REG_S(1);
    GET_FP_REG_S(2);
    GET_FP_REG_S(3);
    GET_FP_REG_S(4);
    GET_FP_REG_S(5);
    GET_FP_REG_S(6);
    GET_FP_REG_S(7);
    GET_FP_REG_S(8);
    GET_FP_REG_S(9);
    GET_FP_REG_S(10);
    GET_FP_REG_S(11);
    GET_FP_REG_S(12);
    GET_FP_REG_S(13);
    GET_FP_REG_S(14);
    GET_FP_REG_S(15);
    GET_FP_REG_S(16);
    GET_FP_REG_S(17);
    GET_FP_REG_S(18);
    GET_FP_REG_S(19);
    GET_FP_REG_S(20);
    GET_FP_REG_S(21);
    GET_FP_REG_S(22);
    GET_FP_REG_S(23);
    GET_FP_REG_S(24);
    GET_FP_REG_S(25);
    GET_FP_REG_S(26);
    GET_FP_REG_S(27);
    GET_FP_REG_S(28);
    GET_FP_REG_S(29);
    GET_FP_REG_S(30);
    GET_FP_REG_S(31);
    GET_FP_REG_D(0);
    GET_FP_REG_D(1);
    GET_FP_REG_D(2);
    GET_FP_REG_D(3);
    GET_FP_REG_D(4);
    GET_FP_REG_D(5);
    GET_FP_REG_D(6);
    GET_FP_REG_D(7);
    GET_FP_REG_D(8);
    GET_FP_REG_D(9);
    GET_FP_REG_D(10);
    GET_FP_REG_D(11);
    GET_FP_REG_D(12);
    GET_FP_REG_D(13);
    GET_FP_REG_D(14);
    GET_FP_REG_D(15);
    GET_FP_REG_D(16);
    GET_FP_REG_D(17);
    GET_FP_REG_D(18);
    GET_FP_REG_D(19);
    GET_FP_REG_D(20);
    GET_FP_REG_D(21);
    GET_FP_REG_D(22);
    GET_FP_REG_D(23);
    GET_FP_REG_D(24);
    GET_FP_REG_D(25);
    GET_FP_REG_D(26);
    GET_FP_REG_D(27);
    GET_FP_REG_D(28);
    GET_FP_REG_D(29);
    GET_FP_REG_D(30);
    GET_FP_REG_D(31);
    default:
      panic("bad fp register");
  }
  return val;
}

void init_fpregs()
{
  PUT_FP_REG(0, 0);
  PUT_FP_REG(1, 0);
  PUT_FP_REG(2, 0);
  PUT_FP_REG(3, 0);
  PUT_FP_REG(4, 0);
  PUT_FP_REG(5, 0);
  PUT_FP_REG(6, 0);
  PUT_FP_REG(7, 0);
  PUT_FP_REG(8, 0);
  PUT_FP_REG(9, 0);
  PUT_FP_REG(10, 0);
  PUT_FP_REG(11, 0);
  PUT_FP_REG(12, 0);
  PUT_FP_REG(13, 0);
  PUT_FP_REG(14, 0);
  PUT_FP_REG(15, 0);
  PUT_FP_REG(16, 0);
  PUT_FP_REG(17, 0);
  PUT_FP_REG(18, 0);
  PUT_FP_REG(19, 0);
  PUT_FP_REG(20, 0);
  PUT_FP_REG(21, 0);
  PUT_FP_REG(22, 0);
  PUT_FP_REG(23, 0);
  PUT_FP_REG(24, 0);
  PUT_FP_REG(25, 0);
  PUT_FP_REG(26, 0);
  PUT_FP_REG(27, 0);
  PUT_FP_REG(28, 0);
  PUT_FP_REG(29, 0);
  PUT_FP_REG(30, 0);
  PUT_FP_REG(31, 0);
}
