// See LICENSE for license details.

#include "encoding.h"

.section .text,"ax",@progbits
.globl _start
_start:

  la sp, stack_top
  la a0, trap_entry
  la gp, _gp
  csrw evec, a0

  li a0, SR_S | SR_PS | SR_EI | SR_S64
  or a1, a0, SR_EF | SR_EA
  csrw status, a1
  csrr a1, status
  csrw status, a0
  
  and a2, a1, SR_EF
  auipc a0, %pcrel_hi(have_fp)
  sw    a2, %lo(have_fp)(a0)
  
  and   a2, a1, SR_EA
  auipc a0, %pcrel_hi(have_accelerator)
  sw    a2, %lo(have_accelerator)(a0)

  call  boot
