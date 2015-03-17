// See LICENSE for license details.

#include "mtrap.h"

#define HANDLE_USER_TRAP_IN_MACHINE_MODE 0       \
  | (0 << (31- 0)) /* IF misaligned */           \
  | (0 << (31- 1)) /* IF fault */                \
  | (1 << (31- 2)) /* illegal instruction */     \
  | (1 << (31- 3)) /* reserved */                \
  | (1 << (31- 4)) /* load misaligned */         \
  | (0 << (31- 5)) /* load fault */              \
  | (1 << (31- 6)) /* store misaligned */        \
  | (0 << (31- 7)) /* store fault */             \
  | (0 << (31- 8)) /* environment call */        \
  | (0 << (31- 9)) /* breakpoint */              \

#define HANDLE_SUPERVISOR_TRAP_IN_MACHINE_MODE 0 \
  | (0 << (31- 0)) /* IF misaligned */           \
  | (0 << (31- 1)) /* IF fault */                \
  | (1 << (31- 2)) /* illegal instruction */     \
  | (1 << (31- 3)) /* reserved */                \
  | (1 << (31- 4)) /* load misaligned */         \
  | (0 << (31- 5)) /* load fault */              \
  | (1 << (31- 6)) /* store misaligned */        \
  | (0 << (31- 7)) /* store fault */             \
  | (1 << (31- 8)) /* environment call */        \
  | (0 << (31- 9)) /* breakpoint */              \

#define HANDLE_MACHINE_TRAP_IN_MACHINE_MODE 0    \
  | (0 << (31- 0)) /* IF misaligned */           \
  | (0 << (31- 1)) /* IF fault */                \
  | (0 << (31- 2)) /* illegal instruction */     \
  | (0 << (31- 3)) /* reserved */                \
  | (0 << (31- 4)) /* load misaligned */         \
  | (1 << (31- 5)) /* load fault */              \
  | (0 << (31- 6)) /* store misaligned */        \
  | (1 << (31- 7)) /* store fault */             \
  | (1 << (31- 8)) /* environment call */        \
  | (0 << (31- 9)) /* breakpoint */              \

  .section .text.init,"ax",@progbits
  .globl mentry
mentry:
  # Entry point from user mode.
  .align 6
  csrrw sp, mscratch, sp
  STORE a0, 10*REGBYTES(sp)
  STORE a1, 11*REGBYTES(sp)

  csrr a0, mcause
  bltz a0, .Linterrupt

  li a1, HANDLE_USER_TRAP_IN_MACHINE_MODE
  SLL32 a1, a1, a0
  bltz a1, .Lhandle_trap_in_machine_mode

  # Redirect the trap to the supervisor.
.Lmrts:
  LOAD a0, 10*REGBYTES(sp)
  LOAD a1, 11*REGBYTES(sp)
  csrrw sp, mscratch, sp
  mrts

  .align 6
  # Entry point from supervisor mode.
  csrrw sp, mscratch, sp
  STORE a0, 10*REGBYTES(sp)
  STORE a1, 11*REGBYTES(sp)

  csrr a0, mcause
  bltz a0, .Linterrupt

  li a1, HANDLE_SUPERVISOR_TRAP_IN_MACHINE_MODE
  SLL32 a1, a1, a0
  bltz a1, .Lhandle_trap_in_machine_mode

.Linterrupt_in_supervisor:
  # Detect double faults.
  csrr a0, mstatus
  SLL32 a0, a0, 31 - CONST_CTZ(MSTATUS_PRV2)
  bltz a0, .Lsupervisor_double_fault

.Lreturn_from_supervisor_double_fault:
  # Redirect the trap to the supervisor.
  LOAD a0, 10*REGBYTES(sp)
  LOAD a1, 11*REGBYTES(sp)
  csrrw sp, mscratch, sp
  mrts

  .align 6
  # Entry point from hypervisor mode.  Not implemented.
  j bad_trap

  .align 6
  csrw mscratch, sp
  addi sp, sp, -INTEGER_CONTEXT_SIZE
  STORE a0,10*REGBYTES(sp)
  STORE a1,11*REGBYTES(sp)

  csrr a0, mcause
  li a1, HANDLE_MACHINE_TRAP_IN_MACHINE_MODE
  SLL32 a1, a1, a0
  bltz a1, .Lhandle_trap_in_machine_mode

  # Uh oh...
.Lbad_trap:
  j bad_trap

.Lsupervisor_double_fault:
  # Return to supervisor trap entry with interrupts disabled.
  # Set PRV2=U, IE2=1, PRV1=S (it already is), and IE1=0.
  li a0, MSTATUS_PRV2 | MSTATUS_IE2 | MSTATUS_IE1
  csrc mstatus, a0
  j .Lreturn_from_supervisor_double_fault

  .align 6
  # Entry point for power-on reset.
  # TODO per-hart stacks
  la sp, _end + RISCV_PGSIZE + 1
  li t0, -RISCV_PGSIZE
  and sp, sp, t0
  j machine_init

  # XXX depend on sbi_base to force its linkage
  la x0, sbi_base

.Linterrupt:
  sll a0, a0, 1    # discard MSB

#if IRQ_TIMER != 0
#error
#endif
  # Send timer interrupts to the OS.
  beqz a0, .Lmrts

  # See if this is an IPI; register a supervisor SW interrupt if so.
  li a1, IRQ_IPI * 2
  bne a0, a1, 1f
  csrrc a0, mstatus, MSTATUS_MSIP
  csrs mstatus, MSTATUS_SSIP

  # There are three cases: PRV1=U; PRV1=S and IE1=1; and PRV1=S and IE1=0.
  # For cases 1-2, do an MRTS; for case 3, we can't, so ERET.
  and a0, a0, MSTATUS_PRV1 | MSTATUS_IE1
  li a1, (MSTATUS_PRV1 & ~(MSTATUS_PRV1<<1)) * PRV_S
  bne a0, a1, .Lmrts

  # And then go back whence we came.
  LOAD a0, 10*REGBYTES(sp)
  LOAD a1, 11*REGBYTES(sp)
  csrrw sp, mscratch, sp
  eret
1:

  # See if this is an HTIF interrupt; if so, handle it in machine mode.
  li a1, IRQ_HOST * 2
  bne a0, a1, .Lbad_trap
  li a0, 10

.Lhandle_trap_in_machine_mode:
  # Preserve the registers.  Compute the address of the trap handler.
  STORE ra, 1*REGBYTES(sp)
  csrr ra, mscratch                # ra <- user sp
  STORE gp, 3*REGBYTES(sp)
  STORE tp, 4*REGBYTES(sp)
  STORE t0, 5*REGBYTES(sp)
1:auipc t0, %pcrel_hi(trap_table)  # t0 <- %hi(trap_table)
  STORE t1, 6*REGBYTES(sp)
  sll t1, a0, 2                    # t1 <- mcause << 2
  STORE t2, 7*REGBYTES(sp)
  add t0, t0, t1                   # t0 <- %hi(trap_table)[mcause]
  STORE s0, 8*REGBYTES(sp)
  lw t0, %pcrel_lo(1b)(t0)         # t0 <- handlers[mcause]
  STORE s1, 9*REGBYTES(sp)
  mv a1, sp                        # a1 <- regs
  STORE a2,12*REGBYTES(sp)
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

  # Restore all of the registers.
  LOAD ra, 1*REGBYTES(sp)
  LOAD gp, 3*REGBYTES(sp)
  LOAD tp, 4*REGBYTES(sp)
  LOAD t0, 5*REGBYTES(sp)
  LOAD t1, 6*REGBYTES(sp)
  LOAD t2, 7*REGBYTES(sp)
  LOAD s0, 8*REGBYTES(sp)
  LOAD s1, 9*REGBYTES(sp)
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

  bnez a0, 1f

  # Go back whence we came.
  LOAD a0, 10*REGBYTES(sp)
  csrw mscratch, sp
  LOAD sp, 2*REGBYTES(sp)
  eret

1:# Redirect the trap to the supervisor.
  LOAD a0, 10*REGBYTES(sp)
  csrw mscratch, sp
  LOAD sp, 2*REGBYTES(sp)
  mrts

  .data
  .align 6
trap_table:
  .word bad_trap
  .word bad_trap
  .word illegal_insn_trap
  .word bad_trap
  .word misaligned_load_trap
  .word machine_page_fault
  .word misaligned_store_trap
  .word machine_page_fault
  .word mcall_trap
  .word bad_trap
  .word htif_interrupt
  .word bad_trap
  .word bad_trap
  .word bad_trap
  .word bad_trap
  .word bad_trap
