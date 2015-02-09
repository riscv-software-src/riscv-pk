// See LICENSE for license details.

#include "encoding.h"

.section .text,"ax",@progbits
.globl _start
_start:
  la gp, _gp
  la sp, stack_top
  csrw mscratch, sp

  li t0, MSTATUS_FS | MSTATUS_XS
  csrs mstatus, t0

  call  boot
