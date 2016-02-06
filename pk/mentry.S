// See LICENSE for license details.

#include "mtrap.h"

  .data
  .align 6
trap_table:
  .word bad_trap
  .word bad_trap
  .word illegal_insn_trap
  .word bad_trap
  .word misaligned_load_trap
  .word bad_trap
  .word misaligned_store_trap
  .word bad_trap
  .word bad_trap
  .word mcall_trap
  .word bad_trap
  .word bad_trap
#define HTIF_INTERRUPT_VECTOR 12
  .word htif_interrupt
#define TRAP_FROM_MACHINE_MODE_VECTOR 13
  .word trap_from_machine_mode
  .word bad_trap
  .word bad_trap

  .option norvc
  .section .text.init,"ax",@progbits
  .globl mentry
mentry:
  .align 6
  # Entry point from user mode (mtvec + 0x000)
  csrrw sp, mscratch, sp
  STORE a0, 10*REGBYTES(sp)
  STORE a1, 11*REGBYTES(sp)

  csrr a1, mcause
  bltz a1, .Linterrupt
  j .Lhandle_trap_in_machine_mode

  .align 6
  # Entry point from supervisor mode (mtvec + 0x040)
  csrrw sp, mscratch, sp
  STORE a0, 10*REGBYTES(sp)
  STORE a1, 11*REGBYTES(sp)

  csrr a1, mcause
  bltz a1, .Linterrupt
  j .Lhandle_trap_in_machine_mode

  .align 6
  # Entry point from hypervisor mode (mtvec + 0x080)
  # Not implemented.
  j bad_trap

  .align 6
  # Entry point from machine mode (mtvec + 0x0C0)
  # These are rare, so punt to C code.
  csrw mscratch, sp
  addi sp, sp, -INTEGER_CONTEXT_SIZE
  STORE a0,10*REGBYTES(sp)
  STORE a1,11*REGBYTES(sp)
  li a1, TRAP_FROM_MACHINE_MODE_VECTOR
  j .Lhandle_trap_in_machine_mode

  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop

  # Entry point for NMIs (mtvec + 0x0FC)
  # Not implemented.
  j bad_trap

  # Entry point for power-on reset (mtvec + 0x100)

  # Zero registers for debuggability
  li x1, 0
  li x2, 0
  li x3, 0
  li x4, 0
  li x5, 0
  li x6, 0
  li x7, 0
  li x8, 0
  li x9, 0
  li x10, 0
  li x11, 0
  li x12, 0
  li x13, 0
  li x14, 0
  li x15, 0
  li x16, 0
  li x17, 0
  li x18, 0
  li x19, 0
  li x20, 0
  li x21, 0
  li x22, 0
  li x23, 0
  li x24, 0
  li x25, 0
  li x26, 0
  li x27, 0
  li x28, 0
  li x29, 0
  li x30, 0
  li x31, 0

  csrr a0, mhartid
  bnez a0, .LmultiHart

  # sp <- end of first full page after the end of the binary
  la sp, _end + 2*RISCV_PGSIZE - 1
  li t0, -RISCV_PGSIZE
  and sp, sp, t0
  addi sp, sp, -MENTRY_FRAME_SIZE

  csrw mscratch, sp
  j init_first_hart

.LmultiHart:
  # make sure our hart id is within a valid range
  li a1, MAX_HARTS
  bgeu a0, a1, .LmultiHart

  # signal we're ready to boot
  csrw mscratch, x0
  fence
  li a1, 1
  sll a1, a1, a0
  la a2, booted_harts_mask
  amoor.w x0, a1, (a2)

  # wait for main hart to grant us a stack
1:csrr sp, mscratch
  beqz sp, 1b

  j init_other_hart

.Linterrupt:
  sll a1, a1, 1    # discard MSB

  # See if this is a timer interrupt; post a supervisor interrupt if so.
  li a0, IRQ_M_TIMER * 2
  bne a0, a1, 1f
  li a0, MIP_MTIP
  csrc mie, a0
  li a0, MIP_STIP
  csrs mip, a0

.Leret:
  # Go back whence we came.
  LOAD a0, 10*REGBYTES(sp)
  LOAD a1, 11*REGBYTES(sp)
  csrrw sp, mscratch, sp
  eret

1:
  # See if this is an IPI; register a supervisor SW interrupt if so.
  li a0, IRQ_M_SOFT * 2
  bne a0, a1, 1f
  csrc mip, MIP_MSIP
  csrs mip, MIP_SSIP
  j .Leret
1:

  # See if this is an HTIF interrupt; if so, handle it in machine mode.
  li a0, IRQ_HOST * 2
  bne a0, a1, .Lbad_trap
  li a1, HTIF_INTERRUPT_VECTOR

.Lhandle_trap_in_machine_mode:
  # Preserve the registers.  Compute the address of the trap handler.
  STORE ra, 1*REGBYTES(sp)
  csrrw ra, mscratch, sp           # ra <- user sp
  STORE gp, 3*REGBYTES(sp)
  STORE tp, 4*REGBYTES(sp)
  STORE t0, 5*REGBYTES(sp)
1:auipc t0, %pcrel_hi(trap_table)  # t0 <- %hi(trap_table)
  STORE t1, 6*REGBYTES(sp)
  sll t1, a1, 2                    # t1 <- mcause << 2
  STORE t2, 7*REGBYTES(sp)
  add t0, t0, t1                   # t0 <- %hi(trap_table)[mcause]
  STORE s0, 8*REGBYTES(sp)
  lw t0, %pcrel_lo(1b)(t0)         # t0 <- trap_table[mcause]
  STORE s1, 9*REGBYTES(sp)
  mv a0, sp                        # a0 <- regs
  STORE a2,12*REGBYTES(sp)
  csrr a2, mepc                    # a2 <- mepc
  STORE a3,13*REGBYTES(sp)
  STORE a4,14*REGBYTES(sp)
  STORE a5,15*REGBYTES(sp)
  STORE a6,16*REGBYTES(sp)
  STORE a7,17*REGBYTES(sp)
  STORE s2,18*REGBYTES(sp)
  STORE s3,19*REGBYTES(sp)
  STORE s4,20*REGBYTES(sp)
  STORE s5,21*REGBYTES(sp)
  STORE s6,22*REGBYTES(sp)
  STORE s7,23*REGBYTES(sp)
  STORE s8,24*REGBYTES(sp)
  STORE s9,25*REGBYTES(sp)
  STORE s10,26*REGBYTES(sp)
  STORE s11,27*REGBYTES(sp)
  STORE t3,28*REGBYTES(sp)
  STORE t4,29*REGBYTES(sp)
  STORE t5,30*REGBYTES(sp)
  STORE t6,31*REGBYTES(sp)
  STORE ra, 2*REGBYTES(sp)         # sp

#ifndef __riscv_hard_float
  lw tp, (sp) # Move the emulated FCSR from x0's save slot into tp.
#endif
  STORE x0, (sp) # Zero x0's save slot.

  # Invoke the handler.
  jalr t0

#ifndef __riscv_hard_float
  sw tp, (sp) # Move the emulated FCSR from tp into x0's save slot.
#endif

restore_regs:
  # Restore all of the registers.
  LOAD ra, 1*REGBYTES(sp)
  LOAD gp, 3*REGBYTES(sp)
  LOAD tp, 4*REGBYTES(sp)
  LOAD t0, 5*REGBYTES(sp)
  LOAD t1, 6*REGBYTES(sp)
  LOAD t2, 7*REGBYTES(sp)
  LOAD s0, 8*REGBYTES(sp)
  LOAD s1, 9*REGBYTES(sp)
  LOAD a0,10*REGBYTES(sp)
  LOAD a1,11*REGBYTES(sp)
  LOAD a2,12*REGBYTES(sp)
  LOAD a3,13*REGBYTES(sp)
  LOAD a4,14*REGBYTES(sp)
  LOAD a5,15*REGBYTES(sp)
  LOAD a6,16*REGBYTES(sp)
  LOAD a7,17*REGBYTES(sp)
  LOAD s2,18*REGBYTES(sp)
  LOAD s3,19*REGBYTES(sp)
  LOAD s4,20*REGBYTES(sp)
  LOAD s5,21*REGBYTES(sp)
  LOAD s6,22*REGBYTES(sp)
  LOAD s7,23*REGBYTES(sp)
  LOAD s8,24*REGBYTES(sp)
  LOAD s9,25*REGBYTES(sp)
  LOAD s10,26*REGBYTES(sp)
  LOAD s11,27*REGBYTES(sp)
  LOAD t3,28*REGBYTES(sp)
  LOAD t4,29*REGBYTES(sp)
  LOAD t5,30*REGBYTES(sp)
  LOAD t6,31*REGBYTES(sp)
  LOAD sp, 2*REGBYTES(sp)
  eret

.globl leave
leave:
  csrr sp, mscratch
  j restore_regs

.Lbad_trap:
  j bad_trap

  # XXX depend on sbi_base to force its linkage
  la x0, sbi_base
