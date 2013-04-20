// See LICENSE for license details.

#include "pcr.h"
#include "pk.h"

.section .text,"ax",@progbits
.globl _start
_start:

  lui sp, %hi(stack_top)
  add sp, sp, %lo(stack_top)

  lui a0, %hi(trap_entry)
  add a0, a0, %lo(trap_entry)
  mtpcr a0, ASM_CR(PCR_EVEC)

#ifdef __riscv64
  li a0, SR_S | SR_PS | SR_ET | SR_EC | SR_S64
#else
  li a0, SR_S | SR_PS | SR_ET | SR_EC
#endif
  or a1, a0, SR_EF | SR_EV
  mtpcr a1, ASM_CR(PCR_SR)
  mfpcr a1, ASM_CR(PCR_SR)
  mtpcr a0, ASM_CR(PCR_SR)
  
  and a2, a1, SR_EF
  lui a0, %hi(have_fp)
  sw  a2, %lo(have_fp)(a0)
  
  and a2, a1, SR_EV
  lui a0, %hi(have_vector)
  sw  a2, %lo(have_vector)(a0)

  lui a0, %hi(boot)
  jalr.j a0, %lo(boot)
  #j boot
