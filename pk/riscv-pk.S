#include "pcr.h"
#include "pk.h"

.section .text,"ax",@progbits
.globl _start
.ent _start
_start:

  lui sp, %hi(stack_top)
  add sp, sp, %lo(stack_top)

  lui t0, %hi(trap_entry)
  add t0, t0, %lo(trap_entry)
  mtpcr t0, ASM_CR(PCR_EVEC)

  li t0, SR_S | SR_PS | SR_ET | SR_EC | SR_S64
  or t1, t0, SR_EF | SR_EV
  mtpcr t1, ASM_CR(PCR_SR)
  mfpcr t1, ASM_CR(PCR_SR)
  mtpcr t0, ASM_CR(PCR_SR)
  
  and t2, t1, SR_EF
  lui t0, %hi(have_fp)
  sw  t2, %lo(have_fp)(t0)
  
  and t2, t1, SR_EV
  lui t0, %hi(have_vector)
  sw  t2, %lo(have_vector)(t0)

  lui t0, %hi(boot)
  jalr.j t0, %lo(boot)
  #j boot

.end _start
