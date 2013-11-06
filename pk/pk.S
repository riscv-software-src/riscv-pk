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
  mtpcr a0, evec

  li a0, SR_S | SR_PS | SR_EI | SR_S64
  or a1, a0, SR_EF | SR_EA
  mtpcr a1, status
  mfpcr a1, status
  mtpcr a0, status
  
  and a2, a1, SR_EF
  lui a0, %hi(have_fp)
  sw  a2, %lo(have_fp)(a0)
  
  and a2, a1, SR_EA
  lui a0, %hi(have_accelerator)
  sw  a2, %lo(have_accelerator)(a0)

  lui a0, %hi(boot)
  jalr a0, %lo(boot)
