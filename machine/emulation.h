#ifndef _RISCV_EMULATION_H
#define _RISCV_EMULATION_H

#include "encoding.h"
#include "bits.h"
#include <stdint.h>

typedef uint32_t insn_t;
typedef void (*emulation_func)(uintptr_t*, uintptr_t, uintptr_t, uintptr_t, insn_t);
#define DECLARE_EMULATION_FUNC(name) void name(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc, uintptr_t mstatus, insn_t insn)

void misaligned_load_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc);
void misaligned_store_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc);
void redirect_trap(uintptr_t epc, uintptr_t mstatus) __attribute__((noreturn));
DECLARE_EMULATION_FUNC(truly_illegal_insn) __attribute__((noreturn));

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

#endif
