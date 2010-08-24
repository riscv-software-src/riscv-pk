#include "pcr.h"

#ifdef PK_ENABLE_KERNEL_64BIT
# define STORE    sd
# define LOAD     ld
# define REGBYTES 8
#else
# define STORE    sw
# define LOAD     lw
# define REGBYTES 4
#endif

	.text
	.set	noat
	.ent	save_tf
save_tf:	# write the trap frame onto the stack

	# save gprs
	STORE	$1,1*REGBYTES($k1)
	STORE	$2,2*REGBYTES($k1)
	STORE	$3,3*REGBYTES($k1)
	STORE	$4,4*REGBYTES($k1)
	STORE	$5,5*REGBYTES($k1)
	STORE	$6,6*REGBYTES($k1)
	STORE	$7,7*REGBYTES($k1)
	STORE	$8,8*REGBYTES($k1)
	STORE	$9,9*REGBYTES($k1)
	STORE	$10,10*REGBYTES($k1)
	STORE	$11,11*REGBYTES($k1)
	STORE	$12,12*REGBYTES($k1)
	STORE	$13,13*REGBYTES($k1)
	STORE	$14,14*REGBYTES($k1)
	STORE	$15,15*REGBYTES($k1)
	STORE	$16,16*REGBYTES($k1)
	STORE	$17,17*REGBYTES($k1)
	STORE	$18,18*REGBYTES($k1)
	STORE	$19,19*REGBYTES($k1)
	STORE	$20,20*REGBYTES($k1)
	STORE	$21,21*REGBYTES($k1)
	STORE	$22,22*REGBYTES($k1)
	STORE	$23,23*REGBYTES($k1)
	STORE	$24,24*REGBYTES($k1)
	STORE	$25,25*REGBYTES($k1)
	STORE	$28,28*REGBYTES($k1)
	STORE	$29,29*REGBYTES($k1)
	STORE	$30,30*REGBYTES($k1)
	STORE	$k0,31*REGBYTES($k1)	# $ra is actually in $k0

	# get sr, epc, badvaddr
	mfpcr	$t0,$PCR_SR					# sr
	STORE	$t0,32*REGBYTES($k1)
	mfpcr	$t0,$PCR_EPC					# epc
	STORE	$t0,33*REGBYTES($k1)
	mfpcr	$t0,$PCR_BADVADDR			# badvaddr
	STORE	$t0,34*REGBYTES($k1)
	jr		$ra
	.end	save_tf

	.globl	pop_tf
	.ent	pop_tf
pop_tf:	# write the trap frame onto the stack
	# restore gprs
	LOAD	$t0,32*REGBYTES($a0)	# restore sr (should disable interrupts)
	mtpcr	$t0,$PCR_SR

	move	$k0,$a0
	LOAD	$1,1*REGBYTES($k0)
	LOAD	$2,2*REGBYTES($k0)
	LOAD	$3,3*REGBYTES($k0)
	LOAD	$4,4*REGBYTES($k0)
	LOAD	$5,5*REGBYTES($k0)
	LOAD	$6,6*REGBYTES($k0)
	LOAD	$7,7*REGBYTES($k0)
	LOAD	$8,8*REGBYTES($k0)
	LOAD	$9,9*REGBYTES($k0)
	LOAD	$10,10*REGBYTES($k0)
	LOAD	$11,11*REGBYTES($k0)
	LOAD	$12,12*REGBYTES($k0)
	LOAD	$13,13*REGBYTES($k0)
	LOAD	$14,14*REGBYTES($k0)
	LOAD	$15,15*REGBYTES($k0)
	LOAD	$16,16*REGBYTES($k0)
	LOAD	$17,17*REGBYTES($k0)
	LOAD	$18,18*REGBYTES($k0)
	LOAD	$19,19*REGBYTES($k0)
	LOAD	$20,20*REGBYTES($k0)
	LOAD	$21,21*REGBYTES($k0)
	LOAD	$22,22*REGBYTES($k0)
	LOAD	$23,23*REGBYTES($k0)
	LOAD	$24,24*REGBYTES($k0)
	LOAD	$25,25*REGBYTES($k0)
	LOAD	$28,28*REGBYTES($k0)
	LOAD	$29,29*REGBYTES($k0)
	LOAD	$30,30*REGBYTES($k0)
	LOAD	$31,31*REGBYTES($k0)

	# gtfo!
	LOAD	$k0,33*REGBYTES($k0)
	mtpcr	$k0,$PCR_EPC
	eret
	.end	pop_tf



	#define TRAP_TABLE_ENTRY(x) \
	.align	7; \
	law		$k1,stack_top-320; \
	move	$k0,$ra; \
	jal		save_tf; \
	move	$sp,$k1; \
	move	$a0,$k1; \
	ei; \
	jal		x; \
	unimp

	.align	12
	.global	trap_table
	.ent	trap_table
trap_table:
	TRAP_TABLE_ENTRY(handle_illegal_instruction)
	TRAP_TABLE_ENTRY(handle_privileged_instruction)
	TRAP_TABLE_ENTRY(handle_fp_disabled)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_misaligned_fetch)
	TRAP_TABLE_ENTRY(handle_misaligned_ldst)
	TRAP_TABLE_ENTRY(handle_fault_fetch)
	TRAP_TABLE_ENTRY(handle_fault_ldst)
	TRAP_TABLE_ENTRY(handle_syscall)
	TRAP_TABLE_ENTRY(handle_breakpoint)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	.align	12
	.end	trap_table

	.bss
	.global stack_bot
	.global stack_top
stack_bot:
	.skip	4096
stack_top:
