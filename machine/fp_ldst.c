#include "fp_emulation.h"
#include "unprivileged_memory.h"

DECLARE_EMULATION_FUNC(emulate_float_load)
{
  uint64_t val;
  uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
  
  switch (insn & MASK_FUNCT3)
  {
    case MATCH_FLW & MASK_FUNCT3:
      if (addr % 4 != 0)
        return misaligned_load_trap(regs, mcause, mepc);

      SET_F32_RD(insn, regs, load_int32_t((void *)addr, mepc));
      break;

    case MATCH_FLD & MASK_FUNCT3:
      if (addr % sizeof(uintptr_t) != 0)
        return misaligned_load_trap(regs, mcause, mepc);

#ifdef __riscv64
      val = load_uint64_t((void *)addr, mepc);
#else
      val = load_uint32_t((void *)addr, mepc);
      val += (uint64_t)load_uint32_t((void *)(addr + 4), mepc) << 32;
#endif
      SET_F64_RD(insn, regs, val);
      break;

    default:
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

DECLARE_EMULATION_FUNC(emulate_float_store)
{
  uint64_t val;
  uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
  
  switch (insn & MASK_FUNCT3)
  {
    case MATCH_FSW & MASK_FUNCT3:
      if (addr % 4 != 0)
        return misaligned_store_trap(regs, mcause, mepc);

      store_uint32_t((void *)addr, GET_F32_RS2(insn, regs), mepc);
      break;

    case MATCH_FSD & MASK_FUNCT3:
      if (addr % sizeof(uintptr_t) != 0)
        return misaligned_store_trap(regs, mcause, mepc);

      val = GET_F64_RS2(insn, regs);
#ifdef __riscv64
      store_uint64_t((void *)addr, val, mepc);
#else
      store_uint32_t((void *)addr, val, mepc);
      store_uint32_t((void *)(addr + 4), val >> 32, mepc);
#endif
      break;

    default:
      return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);
  }
}

