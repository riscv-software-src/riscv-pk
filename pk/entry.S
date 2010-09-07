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
	STORE	$2,2*REGBYTES($1)
	mfpcr	$2,$PCR_K1
	STORE	$2,1*REGBYTES($1)		# $x1 is actually in $PCR_K1
	STORE	$3,3*REGBYTES($1)
	STORE	$4,4*REGBYTES($1)
	STORE	$5,5*REGBYTES($1)
	STORE	$6,6*REGBYTES($1)
	STORE	$7,7*REGBYTES($1)
	STORE	$8,8*REGBYTES($1)
	STORE	$9,9*REGBYTES($1)
	STORE	$10,10*REGBYTES($1)
	STORE	$11,11*REGBYTES($1)
	STORE	$12,12*REGBYTES($1)
	STORE	$13,13*REGBYTES($1)
	STORE	$14,14*REGBYTES($1)
	STORE	$15,15*REGBYTES($1)
	STORE	$16,16*REGBYTES($1)
	STORE	$17,17*REGBYTES($1)
	STORE	$18,18*REGBYTES($1)
	STORE	$19,19*REGBYTES($1)
	STORE	$20,20*REGBYTES($1)
	STORE	$21,21*REGBYTES($1)
	STORE	$22,22*REGBYTES($1)
	STORE	$23,23*REGBYTES($1)
	STORE	$24,24*REGBYTES($1)
	STORE	$25,25*REGBYTES($1)
	STORE	$26,26*REGBYTES($1)
	STORE	$27,27*REGBYTES($1)
	STORE	$28,28*REGBYTES($1)
	STORE	$29,29*REGBYTES($1)
	STORE	$30,30*REGBYTES($1)
	mfpcr	$2,$PCR_K0
	STORE	$2,31*REGBYTES($1)	# $ra is actually in $PCR_K0

	# get sr, epc, badvaddr, cr29 (tid)
	mfpcr	$2,$PCR_SR					# sr
	STORE	$2,32*REGBYTES($1)
	mfpcr	$2,$PCR_EPC					# epc
	STORE	$2,33*REGBYTES($1)
	mfpcr	$2,$PCR_BADVADDR			# badvaddr
	STORE	$2,34*REGBYTES($1)
	mfcr	$2,$29						# cr29 (tid)
	STORE	$2,35*REGBYTES($1)
	jr		$ra
	.end	save_tf

	.globl	pop_tf
	.ent	pop_tf
pop_tf:	# write the trap frame onto the stack
	# restore gprs
	LOAD	$t0,32*REGBYTES($a0)	# restore sr (should disable interrupts)
	mtpcr	$t0,$PCR_SR

	LOAD	$1,1*REGBYTES($a0)
	mtpcr	$1,$PCR_K0
	LOAD	$1,2*REGBYTES($a0)
	mtpcr	$1,$PCR_K1
	move	$1,$a0
	LOAD	$3,3*REGBYTES($1)
	LOAD	$4,4*REGBYTES($1)
	LOAD	$5,5*REGBYTES($1)
	LOAD	$6,6*REGBYTES($1)
	LOAD	$7,7*REGBYTES($1)
	LOAD	$8,8*REGBYTES($1)
	LOAD	$9,9*REGBYTES($1)
	LOAD	$10,10*REGBYTES($1)
	LOAD	$11,11*REGBYTES($1)
	LOAD	$12,12*REGBYTES($1)
	LOAD	$13,13*REGBYTES($1)
	LOAD	$14,14*REGBYTES($1)
	LOAD	$15,15*REGBYTES($1)
	LOAD	$16,16*REGBYTES($1)
	LOAD	$17,17*REGBYTES($1)
	LOAD	$18,18*REGBYTES($1)
	LOAD	$19,19*REGBYTES($1)
	LOAD	$20,20*REGBYTES($1)
	LOAD	$21,21*REGBYTES($1)
	LOAD	$22,22*REGBYTES($1)
	LOAD	$23,23*REGBYTES($1)
	LOAD	$24,24*REGBYTES($1)
	LOAD	$25,25*REGBYTES($1)
	LOAD	$26,26*REGBYTES($1)
	LOAD	$27,27*REGBYTES($1)
	LOAD	$28,28*REGBYTES($1)
	LOAD	$29,29*REGBYTES($1)
	LOAD	$30,30*REGBYTES($1)
	LOAD	$31,31*REGBYTES($1)

	# restore cr29 (tid)
	LOAD	$2,35*REGBYTES($1)
	mtcr	$2,$29

	# gtfo!
	LOAD	$2,33*REGBYTES($1)
	mtpcr	$2,$PCR_EPC
	mfpcr	$1,$PCR_K0
	mfpcr	$2,$PCR_K1
	eret
	.end	pop_tf



	#define TRAP_TABLE_ENTRY(x) \
	.align	7; \
	mtpcr	$ra,$PCR_K0; \
	mtpcr	$1,$PCR_K1; \
	law		$1,stack_top-320; \
	jal		save_tf; \
	move	$sp,$1; \
	move	$a0,$1; \
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
