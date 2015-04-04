#include "mtrap.h"
#include "softfloat.h"
#include <limits.h>

DECLARE_EMULATION_FUNC(truly_illegal_insn)
{
  return -1;
}

uintptr_t misaligned_load_trap(uintptr_t mcause, uintptr_t* regs)
{
  uintptr_t mstatus = read_csr(mstatus);
  uintptr_t mepc = read_csr(mepc);
  insn_fetch_t fetch = get_insn(mcause, mstatus, mepc);
  if (fetch.error)
    return -1;

  uintptr_t val, res, tmp;
  uintptr_t addr = GET_RS1(fetch.insn, regs) + IMM_I(fetch.insn);

  #define DO_LOAD(type_lo, type_hi, insn_lo, insn_hi) ({ \
      type_lo val_lo; \
      type_hi val_hi; \
      uintptr_t addr_lo = addr & -sizeof(type_hi); \
      uintptr_t addr_hi = addr_lo + sizeof(type_hi); \
      uintptr_t masked_addr = sizeof(type_hi) < 4 ? addr % sizeof(type_hi) : addr; \
      res = unpriv_mem_access(mstatus, mepc, \
                              insn_lo " %[val_lo], (%[addr_lo]);" \
                              insn_hi " %[val_hi], (%[addr_hi])", \
                              val_lo, val_hi, addr_lo, addr_hi); \
      val_lo >>= masked_addr * 8; \
      val_hi <<= (sizeof(type_hi) - masked_addr) * 8; \
      val = (type_hi)(val_lo | val_hi); \
  })

  if ((fetch.insn & MASK_LW) == MATCH_LW)
    DO_LOAD(uint32_t, int32_t, "lw", "lw");
#ifdef __riscv64
  else if ((fetch.insn & MASK_LD) == MATCH_LD)
    DO_LOAD(uint64_t, uint64_t, "ld", "ld");
  else if ((fetch.insn & MASK_LWU) == MATCH_LWU)
    DO_LOAD(uint32_t, uint32_t, "lwu", "lwu");
#endif
  else if ((fetch.insn & MASK_FLD) == MATCH_FLD) {
#ifdef __riscv64
    DO_LOAD(uint64_t, uint64_t, "ld", "ld");
    if (res == 0) {
      SET_F64_RD(fetch.insn, regs, val);
      goto success;
    }
#else
    DO_LOAD(uint32_t, int32_t, "lw", "lw");
    if (res == 0) {
      uint64_t double_val = val;
      addr += 4;
      DO_LOAD(uint32_t, int32_t, "lw", "lw");
      double_val |= (uint64_t)val << 32;
      if (res == 0) {
        SET_F64_RD(fetch.insn, regs, val);
        goto success;
      }
    }
#endif
  } else if ((fetch.insn & MASK_FLW) == MATCH_FLW) {
    DO_LOAD(uint32_t, uint32_t, "lw", "lw");
    if (res == 0) {
      SET_F32_RD(fetch.insn, regs, val);
      goto success;
    }
  } else if ((fetch.insn & MASK_LH) == MATCH_LH) {
    // equivalent to DO_LOAD(uint32_t, int16_t, "lhu", "lh")
    res = unpriv_mem_access(mstatus, mepc,
                            "lbu %[val], 0(%[addr]);"
                            "lb %[tmp], 1(%[addr])",
                            val, tmp, addr, mstatus/*X*/);
    val |= tmp << 8;
  } else if ((fetch.insn & MASK_LHU) == MATCH_LHU) {
    // equivalent to DO_LOAD(uint32_t, uint16_t, "lhu", "lhu")
    res = unpriv_mem_access(mstatus, mepc,
                            "lbu %[val], 0(%[addr]);"
                            "lbu %[tmp], 1(%[addr])",
                            val, tmp, addr, mstatus/*X*/);
    val |= tmp << 8;
  } else {
    return -1;
  }

  if (res) {
    restore_mstatus(mstatus, mepc);
    return -1;
  }

  SET_RD(fetch.insn, regs, val);

success:
  write_csr(mepc, mepc + 4);
  return 0;
}

uintptr_t misaligned_store_trap(uintptr_t mcause, uintptr_t* regs)
{
  uintptr_t mstatus = read_csr(mstatus);
  uintptr_t mepc = read_csr(mepc);
  insn_fetch_t fetch = get_insn(mcause, mstatus, mepc);
  if (fetch.error)
    return -1;

  uintptr_t addr = GET_RS1(fetch.insn, regs) + IMM_S(fetch.insn);
  uintptr_t val = GET_RS2(fetch.insn, regs), error;

  if ((fetch.insn & MASK_SW) == MATCH_SW) {
SW:
    error = unpriv_mem_access(mstatus, mepc,
                              "sb %[val], 0(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 1(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 2(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 3(%[addr]);",
                              unused1, unused2, val, addr);
#ifdef __riscv64
  } else if ((fetch.insn & MASK_SD) == MATCH_SD) {
SD:
    error = unpriv_mem_access(mstatus, mepc,
                              "sb %[val], 0(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 1(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 2(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 3(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 4(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 5(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 6(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 7(%[addr]);",
                              unused1, unused2, val, addr);
#endif
  } else if ((fetch.insn & MASK_SH) == MATCH_SH) {
    error = unpriv_mem_access(mstatus, mepc,
                              "sb %[val], 0(%[addr]);"
                              "srl %[val], %[val], 8; sb %[val], 1(%[addr]);",
                              unused1, unused2, val, addr);
  } else if ((fetch.insn & MASK_FSD) == MATCH_FSD) {
#ifdef __riscv64
    val = GET_F64_RS2(fetch.insn, regs);
    goto SD;
#else
    uint64_t double_val = GET_F64_RS2(fetch.insn, regs);
    uint32_t val_lo = double_val, val_hi = double_val >> 32;
    error = unpriv_mem_access(mstatus, mepc,
                              "sb %[val_lo], 0(%[addr]);"
                              "srl %[val_lo], %[val_lo], 8; sb %[val_lo], 1(%[addr]);"
                              "srl %[val_lo], %[val_lo], 8; sb %[val_lo], 2(%[addr]);"
                              "srl %[val_lo], %[val_lo], 8; sb %[val_lo], 3(%[addr]);"
                              "sb %[val_hi], 4(%[addr]);"
                              "srl %[val_hi], %[val_hi], 8; sb %[val_hi], 5(%[addr]);"
                              "srl %[val_hi], %[val_hi], 8; sb %[val_hi], 6(%[addr]);"
                              "srl %[val_hi], %[val_hi], 8; sb %[val_hi], 7(%[addr]);",
                              unused1, unused2, val_lo, val_hi, addr);
#endif
  } else if ((fetch.insn & MASK_FSW) == MATCH_FSW) {
    val = GET_F32_RS2(fetch.insn, regs);
    goto SW;
  } else
    return -1;

  if (error) {
    restore_mstatus(mstatus, mepc);
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_float_load)
{
  uintptr_t val_lo, val_hi, error;
  uint64_t val;
  uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
  
  switch (insn & MASK_FUNCT3)
  {
    case MATCH_FLW & MASK_FUNCT3:
      if (addr % 4 != 0)
        return misaligned_load_trap(mcause, regs);

      error = unpriv_mem_access(mstatus, mepc,
                                "lw %[val_lo], (%[addr])",
                                val_lo, val_hi/*X*/, addr, mstatus/*X*/);

      if (error == 0) {
        SET_F32_RD(insn, regs, val_lo);
        goto success;
      }
      break;

    case MATCH_FLD & MASK_FUNCT3:
      if (addr % sizeof(uintptr_t) != 0)
        return misaligned_load_trap(mcause, regs);
#ifdef __riscv64
      error = unpriv_mem_access(mstatus, mepc,
                                "ld %[val], (%[addr])",
                                val, val_hi/*X*/, addr, mstatus/*X*/);
#else
      error = unpriv_mem_access(mstatus, mepc,
                                "lw %[val_lo], (%[addr]);"
                                "lw %[val_hi], 4(%[addr])",
                                val_lo, val_hi, addr, mstatus/*X*/);
      val = val_lo | ((uint64_t)val_hi << 32);
#endif

      if (error == 0) {
        SET_F64_RD(insn, regs, val);
        goto success;
      }
      break;
  }

  restore_mstatus(mstatus, mepc);
  return -1;

success:
  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_float_store)
{
  uintptr_t val_lo, val_hi, error;
  uint64_t val;
  uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
  
  switch (insn & MASK_FUNCT3)
  {
    case MATCH_FSW & MASK_FUNCT3:
      if (addr % 4 != 0)
        return misaligned_store_trap(mcause, regs);

      val_lo = GET_F32_RS2(insn, regs);
      error = unpriv_mem_access(mstatus, mepc,
                                "sw %[val_lo], (%[addr])",
                                unused1, unused2, val_lo, addr);
      break;

    case MATCH_FSD & MASK_FUNCT3:
      if (addr % sizeof(uintptr_t) != 0)
        return misaligned_store_trap(mcause, regs);

      val = GET_F64_RS2(insn, regs);
#ifdef __riscv64
      error = unpriv_mem_access(mstatus, mepc,
                                "sd %[val], (%[addr])",
                                unused1, unused2, val, addr);
#else
      val_lo = val;
      val_hi = val >> 32;
      error = unpriv_mem_access(mstatus, mepc,
                                "sw %[val_lo], (%[addr]);"
                                "sw %[val_hi], 4(%[addr])",
                                unused1, unused2, val_lo, val_hi, addr);
#endif
      break;

    default:
      error = 1;
  }

  if (error) {
    restore_mstatus(mstatus, mepc);
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

#ifdef __riscv64
typedef __int128 double_int;
#else
typedef int64_t double_int;
#endif

DECLARE_EMULATION_FUNC(emulate_mul_div)
{
  uintptr_t rs1 = GET_RS1(insn, regs), rs2 = GET_RS2(insn, regs), val;

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
    return -1;

  SET_RD(insn, regs, val);
  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_mul_div32)
{
#ifndef __riscv64
  return truly_illegal_insn(mcause, regs, insn, mstatus, mepc);
#endif

  uint32_t rs1 = GET_RS1(insn, regs), rs2 = GET_RS2(insn, regs);
  int32_t val;

  // If compiled with -mno-multiply, GCC will expand these out
  if ((insn & MASK_MUL) == MATCH_MULW)
    val = rs1 * rs2;
  else if ((insn & MASK_DIV) == MATCH_DIV)
    val = (int32_t)rs1 / (int32_t)rs2;
  else if ((insn & MASK_DIVU) == MATCH_DIVU)
    val = rs1 / rs2;
  else if ((insn & MASK_REM) == MATCH_REM)
    val = (int32_t)rs1 % (int32_t)rs2;
  else if ((insn & MASK_REMU) == MATCH_REMU)
    val = rs1 % rs2;
  else
    return -1;

  SET_RD(insn, regs, val);
  write_csr(mepc, mepc + 4);
  return 0;
}

static inline int emulate_read_csr(int num, uintptr_t* result, uintptr_t mstatus)
{
  switch (num)
  {
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
  }
  return -1;
}

static inline int emulate_write_csr(int num, uintptr_t value, uintptr_t mstatus)
{
  switch (num)
  {
    case CSR_FRM: SET_FRM(value); return 0;
    case CSR_FFLAGS: SET_FFLAGS(value); return 0;
    case CSR_FCSR: SET_FCSR(value); return 0;
  }
  return -1;
}

DECLARE_EMULATION_FUNC(emulate_system)
{
  int rs1_num = (insn >> 15) & 0x1f;
  uintptr_t rs1_val = GET_RS1(insn, regs);
  int csr_num = (uint32_t)insn >> 20;
  uintptr_t csr_val, new_csr_val;

  if (emulate_read_csr(csr_num, &csr_val, mstatus) != 0)
    return -1;

  int do_write = rs1_num;
  switch (GET_RM(insn))
  {
    case 0: return -1;
    case 1: new_csr_val = rs1_val; do_write = 1; break;
    case 2: new_csr_val = csr_val | rs1_val; break;
    case 3: new_csr_val = csr_val & ~rs1_val; break;
    case 4: return -1;
    case 5: new_csr_val = rs1_num; do_write = 1; break;
    case 6: new_csr_val = csr_val | rs1_num; break;
    case 7: new_csr_val = csr_val & ~rs1_num; break;
  }

  if (do_write && emulate_write_csr(csr_num, new_csr_val, mstatus) != 0)
    return -1;

  SET_RD(insn, regs, csr_val);
  write_csr(mepc, mepc + 4);
  return 0;
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
    return -1;

  extern int32_t fp_emulation_table[];
  int32_t* pf = (void*)fp_emulation_table + ((insn >> 25) & 0x7c);
  emulation_func f = (emulation_func)(uintptr_t)*pf;

  SETUP_STATIC_ROUNDING(insn);
  return f(mcause, regs, insn, mstatus, mepc);
}

uintptr_t emulate_any_fadd(uintptr_t mcause, uintptr_t* regs, insn_t insn, uintptr_t mstatus, uintptr_t mepc, uintptr_t neg_b)
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
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fadd)
{
  return emulate_any_fadd(mcause, regs, insn, mstatus, mepc, 0);
}

DECLARE_EMULATION_FUNC(emulate_fsub)
{
  return emulate_any_fadd(mcause, regs, insn, mstatus, mepc, INT32_MIN);
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
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
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
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fsqrt)
{
  if ((insn >> 20) & 0x1f)
    return -1;

  if (GET_PRECISION(insn) == PRECISION_S) {
    SET_F32_RD(insn, regs, f32_sqrt(GET_F32_RS1(insn, regs)));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    SET_F64_RD(insn, regs, f64_sqrt(GET_F64_RS1(insn, regs)));
  } else {
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fsgnj)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return -1;

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
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fmin)
{
  int rm = GET_RM(insn);
  if (rm >= 2)
    return -1;

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
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fcvt_ff)
{
  int rs2_num = (insn >> 20) & 0x1f;
  if (GET_PRECISION(insn) == PRECISION_S) {
    if (rs2_num != 1)
      return -1;
    SET_F32_RD(insn, regs, f64_to_f32(GET_F64_RS1(insn, regs)));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    if (rs2_num != 0)
      return -1;
    SET_F64_RD(insn, regs, f32_to_f64(GET_F32_RS1(insn, regs)));
  } else {
    return -1;
  }

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fcvt_fi)
{
  if (GET_PRECISION(insn) != PRECISION_S && GET_PRECISION(insn) != PRECISION_D)
    return -1;

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
      return -1;
  }

  uint64_t float64 = ui64_to_f64(uint_val);
  if (negative)
    float64 ^= INT64_MIN;

  if (GET_PRECISION(insn) == PRECISION_S)
    SET_F32_RD(insn, regs, f64_to_f32(float64));
  else
    SET_F64_RD(insn, regs, float64);

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fcvt_if)
{
  int rs2_num = (insn >> 20) & 0x1f;
#ifdef __riscv64
  if (rs2_num >= 4)
    return -1;
#else
  if (rs2_num >= 2)
    return -1;
#endif

  int64_t float64;
  if (GET_PRECISION(insn) == PRECISION_S)
    float64 = f32_to_f64(GET_F32_RS1(insn, regs));
  else if (GET_PRECISION(insn) == PRECISION_D)
    float64 = GET_F64_RS1(insn, regs);
  else
    return -1;

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
  }

  if (uint_val > limit) {
    result = limit_result;
    softfloat_raiseFlags(softfloat_flag_invalid);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);

  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fcmp)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return -1;

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
  return -1;
success:
  SET_RD(insn, regs, result);
  write_csr(mepc, mepc + 4);
  return 0;
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
    return -1;

  SET_RD(insn, regs, result);
  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fmv_fi)
{
  uintptr_t rs1 = GET_RS1(insn, regs);

  if ((insn & MASK_FMV_S_X) == MATCH_FMV_S_X)
    SET_F32_RD(insn, regs, rs1);
  else if ((insn & MASK_FMV_D_X) == MATCH_FMV_D_X)
    SET_F64_RD(insn, regs, rs1);
  else
    return -1;

  write_csr(mepc, mepc + 4);
  return 0;
}

uintptr_t emulate_any_fmadd(int op, uintptr_t* regs, insn_t insn, uintptr_t mstatus, uintptr_t mepc)
{
  // if FPU is disabled, punt back to the OS
  if (unlikely((mstatus & MSTATUS_FS) == 0))
    return -1;

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
    return -1;
  }
  write_csr(mepc, mepc + 4);
  return 0;
}

DECLARE_EMULATION_FUNC(emulate_fmadd)
{
  int op = 0;
  return emulate_any_fmadd(op, regs, insn, mstatus, mepc);
}

DECLARE_EMULATION_FUNC(emulate_fmsub)
{
  int op = softfloat_mulAdd_subC;
  return emulate_any_fmadd(op, regs, insn, mstatus, mepc);
}

DECLARE_EMULATION_FUNC(emulate_fnmadd)
{
  int op = softfloat_mulAdd_subC | softfloat_mulAdd_subProd;
  return emulate_any_fmadd(op, regs, insn, mstatus, mepc);
}

DECLARE_EMULATION_FUNC(emulate_fnmsub)
{
  int op = softfloat_mulAdd_subProd;
  return emulate_any_fmadd(op, regs, insn, mstatus, mepc);
}
