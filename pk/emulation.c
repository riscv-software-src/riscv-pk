#include "mtrap.h"
#include "softfloat.h"
#include <limits.h>

void redirect_trap(uintptr_t epc, uintptr_t mstatus)
{
  write_csr(sepc, epc);
  write_csr(scause, read_csr(mcause));
  write_csr(mepc, read_csr(stvec));

  uintptr_t prev_priv = EXTRACT_FIELD(mstatus, MSTATUS_MPP);
  uintptr_t prev_ie = EXTRACT_FIELD(mstatus, MSTATUS_MPIE);
  kassert(prev_priv <= PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_SPP, prev_priv);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_SPIE, prev_ie);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPP, PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPIE, 0);
  write_csr(mstatus, mstatus);

  extern void __redirect_trap();
  return __redirect_trap();
}

void __attribute__((noinline)) truly_illegal_insn(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn)
{
  redirect_trap(mepc, mstatus);
}

void misaligned_load_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  uintptr_t mstatus;
  insn_t insn = get_insn(mepc, &mstatus);

  int shift = 0, fp = 0, len;
  if ((insn & MASK_LW) == MATCH_LW)
    len = 4, shift = 8*(sizeof(uintptr_t) - len);
#ifdef __riscv64
  else if ((insn & MASK_LD) == MATCH_LD)
    len = 8, shift = 8*(sizeof(uintptr_t) - len);
  else if ((insn & MASK_LWU) == MATCH_LWU)
    fp = 0, len = 4, shift = 0;
#endif
  else if ((insn & MASK_FLD) == MATCH_FLD)
    fp = 1, len = 8;
  else if ((insn & MASK_FLW) == MATCH_FLW)
    fp = 1, len = 4;
  else if ((insn & MASK_LH) == MATCH_LH)
    len = 2, shift = 8*(sizeof(uintptr_t) - len);
  else if ((insn & MASK_LHU) == MATCH_LHU)
    len = 2;
  else
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
  uintptr_t val = 0, tmp, tmp2;
  unpriv_mem_access("add %[tmp2], %[addr], %[len];"
                    "1: slli %[val], %[val], 8;"
                    "lbu %[tmp], -1(%[tmp2]);"
                    "addi %[tmp2], %[tmp2], -1;"
                    "or %[val], %[val], %[tmp];"
                    "bne %[addr], %[tmp2], 1b;",
                    val, tmp, tmp2, addr, len);

  if (shift)
    val = (intptr_t)val << shift >> shift;

  if (!fp)
    SET_RD(insn, regs, val);
  else if (len == 8)
    SET_F64_RD(insn, regs, val);
  else
    SET_F32_RD(insn, regs, val);

  write_csr(mepc, mepc + 4);
}

void misaligned_store_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  uintptr_t mstatus;
  insn_t insn = get_insn(mepc, &mstatus);

  uintptr_t val = GET_RS2(insn, regs), error;
  int len;
  if ((insn & MASK_SW) == MATCH_SW)
    len = 4;
  else if ((insn & MASK_SD) == MATCH_SD)
    len = 8;
  else if ((insn & MASK_FSD) == MATCH_FSD)
    len = 8, val = GET_F64_RS2(insn, regs);
  else if ((insn & MASK_FSW) == MATCH_FSW)
    len = 4, val = GET_F32_RS2(insn, regs);
  else if ((insn & MASK_SH) == MATCH_SH)
    len = 2;
  else
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
  uintptr_t tmp, tmp2, addr_end = addr + len;
  unpriv_mem_access("mv %[tmp], %[val];"
                    "mv %[tmp2], %[addr];"
                    "1: sb %[tmp], 0(%[tmp2]);"
                    "srli %[tmp], %[tmp], 8;"
                    "addi %[tmp2], %[tmp2], 1;"
                    "bne %[tmp2], %[addr_end], 1b",
                    tmp, tmp2, unused1, val, addr, addr_end);

  write_csr(mepc, mepc + 4);
}

DECLARE_EMULATION_FUNC(emulate_float_load)
{
  uintptr_t val_lo, val_hi;
  uint64_t val;
  uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
  
  switch (insn & MASK_FUNCT3)
  {
    case MATCH_FLW & MASK_FUNCT3:
      if (addr % 4 != 0)
        return misaligned_load_trap(regs, mcause, mepc);

      unpriv_mem_access("lw %[val_lo], (%[addr])",
                        val_lo, unused1, unused2, addr, mepc/*X*/);
      SET_F32_RD(insn, regs, val_lo);
      break;

    case MATCH_FLD & MASK_FUNCT3:
      if (addr % sizeof(uintptr_t) != 0)
        return misaligned_load_trap(regs, mcause, mepc);

#ifdef __riscv64
      unpriv_mem_access("ld %[val], (%[addr])",
                        val, val_hi/*X*/, unused1, addr, mepc/*X*/);
#else
      unpriv_mem_access("lw %[val_lo], (%[addr]);"
                        "lw %[val_hi], 4(%[addr])",
                        val_lo, val_hi, unused1, addr, mepc/*X*/);
      val = val_lo | ((uint64_t)val_hi << 32);
#endif
      SET_F64_RD(insn, regs, val);
      break;

    default:
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_float_store)
{
  uintptr_t val_lo, val_hi;
  uint64_t val;
  uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
  
  switch (insn & MASK_FUNCT3)
  {
    case MATCH_FSW & MASK_FUNCT3:
      if (addr % 4 != 0)
        return misaligned_store_trap(regs, mcause, mepc);

      val_lo = GET_F32_RS2(insn, regs);
      unpriv_mem_access("sw %[val_lo], (%[addr])",
                        unused1, unused2, unused3, val_lo, addr);
      break;

    case MATCH_FSD & MASK_FUNCT3:
      if (addr % sizeof(uintptr_t) != 0)
        return misaligned_store_trap(regs, mcause, mepc);

      val = GET_F64_RS2(insn, regs);
#ifdef __riscv64
      unpriv_mem_access("sd %[val], (%[addr])",
                        unused1, unused2, unused3, val, addr);
#else
      val_lo = val;
      val_hi = val >> 32;
      unpriv_mem_access("sw %[val_lo], (%[addr]);"
                        "sw %[val_hi], 4(%[addr])",
                        unused1, unused2, unused3, val_lo, val_hi, addr);
#endif
      break;

    default:
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

#ifdef __riscv64
typedef __int128 double_int;
#else
typedef int64_t double_int;
#endif

DECLARE_EMULATION_FUNC(emulate_mul_div)
{
  uintptr_t rs1 = GET_RS1(insn, regs), rs2 = GET_RS2(insn, regs), val;

#ifndef __riscv_muldiv
  // If compiled with -mno-multiply, GCC will expand these out
  if ((insn & MASK_MUL) == MATCH_MUL)
    val = rs1 * rs2;
  else if ((insn & MASK_DIV) == MATCH_DIV)
    val = (intptr_t)rs1 / (intptr_t)rs2;
  else if ((insn & MASK_DIVU) == MATCH_DIVU)
    val = rs1 / rs2;
  else if ((insn & MASK_REM) == MATCH_REM)
    val = (intptr_t)rs1 % (intptr_t)rs2;
  else if ((insn & MASK_REMU) == MATCH_REMU)
    val = rs1 % rs2;
  else if ((insn & MASK_MULH) == MATCH_MULH)
    val = ((double_int)(intptr_t)rs1 * (double_int)(intptr_t)rs2) >> (8 * sizeof(rs1));
  else if ((insn & MASK_MULHU) == MATCH_MULHU)
    val = ((double_int)rs1 * (double_int)rs2) >> (8 * sizeof(rs1));
  else if ((insn & MASK_MULHSU) == MATCH_MULHSU)
    val = ((double_int)(intptr_t)rs1 * (double_int)rs2) >> (8 * sizeof(rs1));
  else
#endif
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  SET_RD(insn, regs, val);
}

DECLARE_EMULATION_FUNC(emulate_mul_div32)
{
  uint32_t rs1 = GET_RS1(insn, regs), rs2 = GET_RS2(insn, regs);
  int32_t val;

#if defined(__riscv64) && !defined(__riscv_muldiv)
  // If compiled with -mno-multiply, GCC will expand these out
  if ((insn & MASK_MULW) == MATCH_MULW)
    val = rs1 * rs2;
  else if ((insn & MASK_DIVW) == MATCH_DIVW)
    val = (int32_t)rs1 / (int32_t)rs2;
  else if ((insn & MASK_DIVUW) == MATCH_DIVUW)
    val = rs1 / rs2;
  else if ((insn & MASK_REMW) == MATCH_REMW)
    val = (int32_t)rs1 % (int32_t)rs2;
  else if ((insn & MASK_REMUW) == MATCH_REMUW)
    val = rs1 % rs2;
  else
#endif
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  SET_RD(insn, regs, val);
}

static inline int emulate_read_csr(int num, uintptr_t mstatus, uintptr_t* result)
{
  switch (num)
  {
    case CSR_TIME:
      *result = read_csr(mtime) + HLS()->utime_delta;
      return 0;
    case CSR_CYCLE:
      *result = read_csr(mcycle) + HLS()->ucycle_delta;
      return 0;
    case CSR_INSTRET:
      *result = read_csr(minstret) + HLS()->uinstret_delta;
      return 0;
    case CSR_STIME:
      *result = read_csr(mtime) + HLS()->stime_delta;
      return 0;
    case CSR_SCYCLE:
      *result = read_csr(mcycle) + HLS()->scycle_delta;
      return 0;
    case CSR_SINSTRET:
      *result = read_csr(minstret) + HLS()->sinstret_delta;
      return 0;
#ifdef __riscv32
    case CSR_TIMEH:
      *result = (((uint64_t)read_csr(mtimeh) << 32) + read_csr(mtime)
                 + HLS()->stime_delta) >> 32;
      return 0;
    case CSR_CYCLEH:
      *result = (((uint64_t)read_csr(mcycleh) << 32) + read_csr(mcycle)
                 + HLS()->scycle_delta) >> 32;
      return 0;
    case CSR_INSTRETH:
      *result = (((uint64_t)read_csr(minstreth) << 32) + read_csr(minstret)
                 + HLS()->sinstret_delta) >> 32;
      return 0;
#endif
#ifndef __riscv_hard_float
    case CSR_FRM:
      if ((mstatus & MSTATUS_FS) == 0) break;
      *result = GET_FRM();
      return 0;
    case CSR_FFLAGS:
      if ((mstatus & MSTATUS_FS) == 0) break;
      *result = GET_FFLAGS();
      return 0;
    case CSR_FCSR:
      if ((mstatus & MSTATUS_FS) == 0) break;
      *result = GET_FCSR();
      return 0;
#endif
  }
  return -1;
}

static inline int emulate_write_csr(int num, uintptr_t value, uintptr_t mstatus)
{
  switch (num)
  {
#ifndef __riscv_hard_float
    case CSR_FRM: SET_FRM(value); return 0;
    case CSR_FFLAGS: SET_FFLAGS(value); return 0;
    case CSR_FCSR: SET_FCSR(value); return 0;
#endif
  }
  return -1;
}

DECLARE_EMULATION_FUNC(emulate_system)
{
  int rs1_num = (insn >> 15) & 0x1f;
  uintptr_t rs1_val = GET_RS1(insn, regs);
  int csr_num = (uint32_t)insn >> 20;
  uintptr_t csr_val, new_csr_val;

  if (emulate_read_csr(csr_num, mstatus, &csr_val))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  int do_write = rs1_num;
  switch (GET_RM(insn))
  {
    case 0: return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    case 1: new_csr_val = rs1_val; do_write = 1; break;
    case 2: new_csr_val = csr_val | rs1_val; break;
    case 3: new_csr_val = csr_val & ~rs1_val; break;
    case 4: return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    case 5: new_csr_val = rs1_num; do_write = 1; break;
    case 6: new_csr_val = csr_val | rs1_num; break;
    case 7: new_csr_val = csr_val & ~rs1_num; break;
  }

  if (do_write && emulate_write_csr(csr_num, new_csr_val, mstatus))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  SET_RD(insn, regs, csr_val);
}

DECLARE_EMULATION_FUNC(emulate_fp)
{
  asm (".pushsection .rodata\n"
       "fp_emulation_table:\n"
       "  .word emulate_fadd\n"
       "  .word emulate_fsub\n"
       "  .word emulate_fmul\n"
       "  .word emulate_fdiv\n"
       "  .word emulate_fsgnj\n"
       "  .word emulate_fmin\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fcvt_ff\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fsqrt\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fcmp\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fcvt_if\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fcvt_fi\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fmv_if\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_fmv_fi\n"
       "  .word truly_illegal_insn\n"
       "  .popsection");

  // if FPU is disabled, punt back to the OS
  if (unlikely((mstatus & MSTATUS_FS) == 0))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  extern int32_t fp_emulation_table[];
  int32_t* pf = (void*)fp_emulation_table + ((insn >> 25) & 0x7c);
  emulation_func f = (emulation_func)(uintptr_t)*pf;

  SETUP_STATIC_ROUNDING(insn);
  return f(regs, mcause, mepc, mstatus, insn);
}

void emulate_any_fadd(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn, int32_t neg_b)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs) ^ neg_b;
    SET_F32_RD(insn, regs, f32_add(rs1, rs2));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs) ^ ((uint64_t)neg_b << 32);
    SET_F64_RD(insn, regs, f64_add(rs1, rs2));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_fadd)
{
  return emulate_any_fadd(regs, mcause, mepc, mstatus, insn, 0);
}

DECLARE_EMULATION_FUNC(emulate_fsub)
{
  return emulate_any_fadd(regs, mcause, mepc, mstatus, insn, INT32_MIN);
}

DECLARE_EMULATION_FUNC(emulate_fmul)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_mul(rs1, rs2));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_mul(rs1, rs2));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_fdiv)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_div(rs1, rs2));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_div(rs1, rs2));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_fsqrt)
{
  if ((insn >> 20) & 0x1f)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  if (GET_PRECISION(insn) == PRECISION_S) {
    SET_F32_RD(insn, regs, f32_sqrt(GET_F32_RS1(insn, regs)));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    SET_F64_RD(insn, regs, f64_sqrt(GET_F64_RS1(insn, regs)));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_fsgnj)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  #define DO_FSGNJ(rs1, rs2, rm) ({ \
    typeof(rs1) rs1_sign = (rs1) >> (8*sizeof(rs1)-1); \
    typeof(rs1) rs2_sign = (rs2) >> (8*sizeof(rs1)-1); \
    rs1_sign &= (rm) >> 1; \
    rs1_sign ^= (rm) ^ rs2_sign; \
    ((rs1) << 1 >> 1) | (rs1_sign << (8*sizeof(rs1)-1)); })

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_fmin)
{
  int rm = GET_RM(insn);
  if (rm >= 2)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    uint32_t arg1 = rm ? rs2 : rs1;
    uint32_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f32_lt_quiet(arg1, arg2) || isNaNF32UI(rs2);
    SET_F32_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    uint64_t arg1 = rm ? rs2 : rs1;
    uint64_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f64_lt_quiet(arg1, arg2) || isNaNF64UI(rs2);
    SET_F64_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_fcvt_ff)
{
  int rs2_num = (insn >> 20) & 0x1f;
  if (GET_PRECISION(insn) == PRECISION_S) {
    if (rs2_num != 1)
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    SET_F32_RD(insn, regs, f64_to_f32(GET_F64_RS1(insn, regs)));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    if (rs2_num != 0)
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
    SET_F64_RD(insn, regs, f32_to_f64(GET_F32_RS1(insn, regs)));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_fcvt_fi)
{
  if (GET_PRECISION(insn) != PRECISION_S && GET_PRECISION(insn) != PRECISION_D)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  int negative = 0;
  uint64_t uint_val = GET_RS1(insn, regs);

  switch ((insn >> 20) & 0x1f)
  {
    case 0: // int32
      negative = (int32_t)uint_val < 0;
      uint_val = negative ? -(int32_t)uint_val : (int32_t)uint_val;
      break;
    case 1: // uint32
      uint_val = (uint32_t)uint_val;
      break;
#ifdef __riscv64
    case 2: // int64
      negative = (int64_t)uint_val < 0;
      uint_val = negative ? -uint_val : uint_val;
    case 3: // uint64
      break;
#endif
    default:
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }

  uint64_t float64 = ui64_to_f64(uint_val);
  if (negative)
    float64 ^= INT64_MIN;

  if (GET_PRECISION(insn) == PRECISION_S)
    SET_F32_RD(insn, regs, f64_to_f32(float64));
  else
    SET_F64_RD(insn, regs, float64);
}

DECLARE_EMULATION_FUNC(emulate_fcvt_if)
{
  int rs2_num = (insn >> 20) & 0x1f;
#ifdef __riscv64
  if (rs2_num >= 4)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
#else
  if (rs2_num >= 2)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
#endif

  int64_t float64;
  if (GET_PRECISION(insn) == PRECISION_S)
    float64 = f32_to_f64(GET_F32_RS1(insn, regs));
  else if (GET_PRECISION(insn) == PRECISION_D)
    float64 = GET_F64_RS1(insn, regs);
  else
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  int negative = 0;
  if (float64 < 0) {
    negative = 1;
    float64 ^= INT64_MIN;
  }
  uint64_t uint_val = f64_to_ui64(float64, softfloat_roundingMode, true);
  uint64_t result, limit, limit_result;

  switch (rs2_num)
  {
    case 0: // int32
      if (negative) {
        result = (int32_t)-uint_val;
        limit_result = limit = (uint32_t)INT32_MIN;
      } else {
        result = (int32_t)uint_val;
        limit_result = limit = INT32_MAX;
      }
      break;

    case 1: // uint32
      limit = limit_result = UINT32_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint32_t)uint_val;
      break;

    case 2: // int32
      if (negative) {
        result = (int64_t)-uint_val;
        limit_result = limit = (uint64_t)INT64_MIN;
      } else {
        result = (int64_t)uint_val;
        limit_result = limit = INT64_MAX;
      }
      break;

    case 3: // uint64
      limit = limit_result = UINT64_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint64_t)uint_val;
      break;

    default:
      __builtin_unreachable();
  }

  if (uint_val > limit) {
    result = limit_result;
    softfloat_raiseFlags(softfloat_flag_invalid);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);
}

DECLARE_EMULATION_FUNC(emulate_fcmp)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  uintptr_t result;
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    if (rm != 1)
      result = f32_eq(rs1, rs2);
    if (rm == 1 || (rm == 0 && !result))
      result = f32_lt(rs1, rs2);
    goto success;
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    if (rm != 1)
      result = f64_eq(rs1, rs2);
    if (rm == 1 || (rm == 0 && !result))
      result = f64_lt(rs1, rs2);
    goto success;
  }
  return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
success:
  SET_RD(insn, regs, result);
}

DECLARE_EMULATION_FUNC(emulate_fmv_if)
{
  uintptr_t result;
  if ((insn & MASK_FMV_X_S) == MATCH_FMV_X_S)
    result = GET_F32_RS1(insn, regs);
#ifdef __riscv64
  else if ((insn & MASK_FMV_X_D) == MATCH_FMV_X_D)
    result = GET_F64_RS1(insn, regs);
#endif
  else
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  SET_RD(insn, regs, result);
}

DECLARE_EMULATION_FUNC(emulate_fmv_fi)
{
  uintptr_t rs1 = GET_RS1(insn, regs);

  if ((insn & MASK_FMV_S_X) == MATCH_FMV_S_X)
    SET_F32_RD(insn, regs, rs1);
  else if ((insn & MASK_FMV_D_X) == MATCH_FMV_D_X)
    SET_F64_RD(insn, regs, rs1);
  else
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
}

DECLARE_EMULATION_FUNC(emulate_fmadd)
{
  // if FPU is disabled, punt back to the OS
  if (unlikely((mstatus & MSTATUS_FS) == 0))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  int op = (insn >> 2) & 3;
  SETUP_STATIC_ROUNDING(insn);
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    uint32_t rs3 = GET_F32_RS3(insn, regs);
    SET_F32_RD(insn, regs, softfloat_mulAddF32(op, rs1, rs2, rs3));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    uint64_t rs3 = GET_F64_RS3(insn, regs);
    SET_F64_RD(insn, regs, softfloat_mulAddF64(op, rs1, rs2, rs3));
  } else {
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}
