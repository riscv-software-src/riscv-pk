#include "emulation.h"
#include "fp_emulation.h"
#include "config.h"
#include "unprivileged_memory.h"
#include "mtrap.h"
#include <limits.h>

void illegal_insn_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  asm (".pushsection .rodata\n"
       "illegal_insn_trap_table:\n"
       "  .word truly_illegal_insn\n"
#ifdef PK_ENABLE_FP_EMULATION
       "  .word emulate_float_load\n"
#else
       "  .word truly_illegal_insn\n"
#endif
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
#ifdef PK_ENABLE_FP_EMULATION
       "  .word emulate_float_store\n"
#else
       "  .word truly_illegal_insn\n"
#endif
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_mul_div\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_mul_div32\n"
       "  .word truly_illegal_insn\n"
#ifdef PK_ENABLE_FP_EMULATION
       "  .word emulate_fmadd\n"
       "  .word emulate_fmadd\n"
       "  .word emulate_fmadd\n"
       "  .word emulate_fmadd\n"
       "  .word emulate_fp\n"
#else
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
#endif
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word emulate_system\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .word truly_illegal_insn\n"
       "  .popsection");

  uintptr_t mstatus;
  insn_t insn = get_insn(mepc, &mstatus);

  if (unlikely((insn & 3) != 3))
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  write_csr(mepc, mepc + 4);

  extern int32_t illegal_insn_trap_table[];
  int32_t* pf = (void*)illegal_insn_trap_table + (insn & 0x7c);
  emulation_func f = (emulation_func)(uintptr_t)*pf;
  f(regs, mcause, mepc, mstatus, insn);
}

void __attribute__((noinline)) truly_illegal_insn(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn)
{
  redirect_trap(mepc, mstatus);
}

void misaligned_load_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  union {
    uint8_t bytes[8];
    uintptr_t intx;
    uint64_t int64;
  } val;
  uintptr_t mstatus;
  insn_t insn = get_insn(mepc, &mstatus);
  uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);

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

  val.int64 = 0;
  for (intptr_t i = len-1; i >= 0; i--)
    val.bytes[i] = load_uint8_t((void *)(addr + i), mepc);

  if (!fp)
    SET_RD(insn, regs, (intptr_t)val.intx << shift >> shift);
  else if (len == 8)
    SET_F64_RD(insn, regs, val.int64);
  else
    SET_F32_RD(insn, regs, val.intx);

  write_csr(mepc, mepc + 4);
}

void misaligned_store_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  union {
    uint8_t bytes[8];
    uintptr_t intx;
    uint64_t int64;
  } val;
  uintptr_t mstatus;
  insn_t insn = get_insn(mepc, &mstatus);
  int len;

  val.intx = GET_RS2(insn, regs);
  if ((insn & MASK_SW) == MATCH_SW)
    len = 4;
#ifdef __riscv64
  else if ((insn & MASK_SD) == MATCH_SD)
    len = 8;
#endif
  else if ((insn & MASK_FSD) == MATCH_FSD)
    len = 8, val.int64 = GET_F64_RS2(insn, regs);
  else if ((insn & MASK_FSW) == MATCH_FSW)
    len = 4, val.intx = GET_F32_RS2(insn, regs);
  else if ((insn & MASK_SH) == MATCH_SH)
    len = 2;
  else
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
  for (size_t i = 0; i < len; i++)
    store_uint8_t((void *)(addr + i), val.bytes[i], mepc);

  write_csr(mepc, mepc + 4);
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
#ifdef PK_ENABLE_FP_EMULATION
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
#ifndef PK_ENABLE_FP_EMULATION
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
