#include "pcr.h"

#ifdef __riscv64
# define STORE    sd
# define LOAD     ld
# define REGBYTES 8
#else
# define STORE    sw
# define LOAD     lw
# define REGBYTES 4
#endif

  .text
  .ent  save_tf
save_tf:  # write the trap frame onto the stack

  # save gprs
  STORE  $x3,3*REGBYTES($x2)
  STORE  $x4,4*REGBYTES($x2)
  STORE  $x5,5*REGBYTES($x2)
  STORE  $x6,6*REGBYTES($x2)
  STORE  $x7,7*REGBYTES($x2)
  STORE  $x8,8*REGBYTES($x2)
  STORE  $x9,9*REGBYTES($x2)
  STORE  $x10,10*REGBYTES($x2)
  STORE  $x11,11*REGBYTES($x2)
  STORE  $x12,12*REGBYTES($x2)
  STORE  $x13,13*REGBYTES($x2)
  STORE  $x14,14*REGBYTES($x2)
  STORE  $x15,15*REGBYTES($x2)
  STORE  $x16,16*REGBYTES($x2)
  STORE  $x17,17*REGBYTES($x2)
  STORE  $x18,18*REGBYTES($x2)
  STORE  $x19,19*REGBYTES($x2)
  STORE  $x20,20*REGBYTES($x2)
  STORE  $x21,21*REGBYTES($x2)
  STORE  $x22,22*REGBYTES($x2)
  STORE  $x23,23*REGBYTES($x2)
  STORE  $x24,24*REGBYTES($x2)
  STORE  $x25,25*REGBYTES($x2)
  STORE  $x26,26*REGBYTES($x2)
  STORE  $x27,27*REGBYTES($x2)
  STORE  $x28,28*REGBYTES($x2)
  STORE  $x29,29*REGBYTES($x2)
  STORE  $x30,30*REGBYTES($x2)
  STORE  $x31,31*REGBYTES($x2)

  mfpcr  $x3,ASM_CR(PCR_K0)
  STORE  $x3,1*REGBYTES($x2)          # $x1 is in $PCR_K0
  mfpcr  $x3,ASM_CR(PCR_K1)
  STORE  $x3,2*REGBYTES($x2)          # $x2 is in $PCR_K1

  # get sr, epc, badvaddr, cause
  mfpcr  $x3,ASM_CR(PCR_SR)          # sr
  STORE  $x3,32*REGBYTES($x2)
  mfpcr  $x4,ASM_CR(PCR_EPC)          # epc
  STORE  $x4,33*REGBYTES($x2)
  mfpcr  $x3,ASM_CR(PCR_BADVADDR)      # badvaddr
  STORE  $x3,34*REGBYTES($x2)
  mfpcr  $x3,ASM_CR(PCR_CAUSE)        # cause
  STORE  $x3,35*REGBYTES($x2)

  # get faulting insn, if it wasn't a fetch-related trap
  li    $x5, CAUSE_MISALIGNED_FETCH
  li    $x6, CAUSE_FAULT_FETCH
  beq   $x3, $x5, 1f
  beq   $x3, $x6, 1f
  lh    $x3,0($x4)
  lh    $x4,2($x4)
  sh    $x3,  36*REGBYTES($x2)
  sh    $x4,2+36*REGBYTES($x2)
1:
  ret
  .end  save_tf

  .globl  pop_tf
  .ent  pop_tf
pop_tf:  # write the trap frame onto the stack
  # restore gprs
  LOAD  $t0,32*REGBYTES($a0)  # restore sr (should disable interrupts)
  mtpcr  $t0,ASM_CR(PCR_SR)

  LOAD  $x1,1*REGBYTES($a0)
  mtpcr  $x1,ASM_CR(PCR_K0)
  LOAD  $x1,2*REGBYTES($a0)
  mtpcr $x1,ASM_CR(PCR_K1)
  move  $x1,$a0
  LOAD  $x3,3*REGBYTES($x1)
  LOAD  $x4,4*REGBYTES($x1)
  LOAD  $x5,5*REGBYTES($x1)
  LOAD  $x6,6*REGBYTES($x1)
  LOAD  $x7,7*REGBYTES($x1)
  LOAD  $x8,8*REGBYTES($x1)
  LOAD  $x9,9*REGBYTES($x1)
  LOAD  $x10,10*REGBYTES($x1)
  LOAD  $x11,11*REGBYTES($x1)
  LOAD  $x12,12*REGBYTES($x1)
  LOAD  $x13,13*REGBYTES($x1)
  LOAD  $x14,14*REGBYTES($x1)
  LOAD  $x15,15*REGBYTES($x1)
  LOAD  $x16,16*REGBYTES($x1)
  LOAD  $x17,17*REGBYTES($x1)
  LOAD  $x18,18*REGBYTES($x1)
  LOAD  $x19,19*REGBYTES($x1)
  LOAD  $x20,20*REGBYTES($x1)
  LOAD  $x21,21*REGBYTES($x1)
  LOAD  $x22,22*REGBYTES($x1)
  LOAD  $x23,23*REGBYTES($x1)
  LOAD  $x24,24*REGBYTES($x1)
  LOAD  $x25,25*REGBYTES($x1)
  LOAD  $x26,26*REGBYTES($x1)
  LOAD  $x27,27*REGBYTES($x1)
  LOAD  $x28,28*REGBYTES($x1)
  LOAD  $x29,29*REGBYTES($x1)
  LOAD  $x30,30*REGBYTES($x1)
  LOAD  $x31,31*REGBYTES($x1)

  # gtfo!
  LOAD  $x2,33*REGBYTES($x1)
  mtpcr $x2,ASM_CR(PCR_EPC)
  mfpcr $x1,ASM_CR(PCR_K0)
  mfpcr $x2,ASM_CR(PCR_K1)
  eret
  .end  pop_tf

  .global  trap_entry
  .ent  trap_entry
trap_entry:
  mtpcr $ra,ASM_CR(PCR_K0)
  mtpcr $x2,ASM_CR(PCR_K1)

  # when coming from kernel, continue below its stack
  mfpcr $ra,ASM_CR(PCR_SR)
  and   $ra,$ra,SR_PS
  add   $x2, $sp, -320
  bnez  $ra, 1f
  la    $x2,stack_top-320

1:jal   save_tf
  move  $sp,$x2
  move  $a0,$x2
  jal    handle_trap
  .end  trap_entry

  .bss
  .global stack_bot
  .global stack_top
stack_bot:
  .skip  4096
stack_top:
