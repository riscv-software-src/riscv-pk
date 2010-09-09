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
	STORE	$x2,2*REGBYTES($x1)
	mfpcr	$x2,ASM_CR(PCR_K1)
	STORE	$x2,1*REGBYTES($x1)		# $x1 is actually in $PCR_K1
	STORE	$x3,3*REGBYTES($x1)
	STORE	$x4,4*REGBYTES($x1)
	STORE	$x5,5*REGBYTES($x1)
	STORE	$x6,6*REGBYTES($x1)
	STORE	$x7,7*REGBYTES($x1)
	STORE	$x8,8*REGBYTES($x1)
	STORE	$x9,9*REGBYTES($x1)
	STORE	$x10,10*REGBYTES($x1)
	STORE	$x11,11*REGBYTES($x1)
	STORE	$x12,12*REGBYTES($x1)
	STORE	$x13,13*REGBYTES($x1)
	STORE	$x14,14*REGBYTES($x1)
	STORE	$x15,15*REGBYTES($x1)
	STORE	$x16,16*REGBYTES($x1)
	STORE	$x17,17*REGBYTES($x1)
	STORE	$x18,18*REGBYTES($x1)
	STORE	$x19,19*REGBYTES($x1)
	STORE	$x20,20*REGBYTES($x1)
	STORE	$x21,21*REGBYTES($x1)
	STORE	$x22,22*REGBYTES($x1)
	STORE	$x23,23*REGBYTES($x1)
	STORE	$x24,24*REGBYTES($x1)
	STORE	$x25,25*REGBYTES($x1)
	STORE	$x26,26*REGBYTES($x1)
	STORE	$x27,27*REGBYTES($x1)
	STORE	$x28,28*REGBYTES($x1)
	STORE	$x29,29*REGBYTES($x1)
	STORE	$x30,30*REGBYTES($x1)
	mfpcr	$x2,ASM_CR(PCR_K0)
	STORE	$x2,31*REGBYTES($x1)	# $ra is actually in $PCR_K0

	# get sr, epc, badvaddr, cr29 (tid)
	mfpcr	$x2,ASM_CR(PCR_SR)					# sr
	STORE	$x2,32*REGBYTES($x1)
	mfpcr	$x2,ASM_CR(PCR_EPC)					# epc
	STORE	$x2,33*REGBYTES($x1)
	mfpcr	$x2,ASM_CR(PCR_BADVADDR)				# badvaddr
	STORE	$x2,34*REGBYTES($x1)
	mfcr	$x2,ASM_CR(29)						# cr29 (tid)
	STORE	$x2,35*REGBYTES($x1)
	jr		$ra
	.end	save_tf

	.globl	pop_tf
	.ent	pop_tf
pop_tf:	# write the trap frame onto the stack
	# restore gprs
	LOAD	$t0,32*REGBYTES($a0)	# restore sr (should disable interrupts)
	mtpcr	$t0,ASM_CR(PCR_SR)

	LOAD	$x1,1*REGBYTES($a0)
	mtpcr	$x1,ASM_CR(PCR_K0)
	LOAD	$x1,2*REGBYTES($a0)
	mtpcr	$x1,ASM_CR(PCR_K1)
	move	$x1,$a0
	LOAD	$x3,3*REGBYTES($x1)
	LOAD	$x4,4*REGBYTES($x1)
	LOAD	$x5,5*REGBYTES($x1)
	LOAD	$x6,6*REGBYTES($x1)
	LOAD	$x7,7*REGBYTES($x1)
	LOAD	$x8,8*REGBYTES($x1)
	LOAD	$x9,9*REGBYTES($x1)
	LOAD	$x10,10*REGBYTES($x1)
	LOAD	$x11,11*REGBYTES($x1)
	LOAD	$x12,12*REGBYTES($x1)
	LOAD	$x13,13*REGBYTES($x1)
	LOAD	$x14,14*REGBYTES($x1)
	LOAD	$x15,15*REGBYTES($x1)
	LOAD	$x16,16*REGBYTES($x1)
	LOAD	$x17,17*REGBYTES($x1)
	LOAD	$x18,18*REGBYTES($x1)
	LOAD	$x19,19*REGBYTES($x1)
	LOAD	$x20,20*REGBYTES($x1)
	LOAD	$x21,21*REGBYTES($x1)
	LOAD	$x22,22*REGBYTES($x1)
	LOAD	$x23,23*REGBYTES($x1)
	LOAD	$x24,24*REGBYTES($x1)
	LOAD	$x25,25*REGBYTES($x1)
	LOAD	$x26,26*REGBYTES($x1)
	LOAD	$x27,27*REGBYTES($x1)
	LOAD	$x28,28*REGBYTES($x1)
	LOAD	$x29,29*REGBYTES($x1)
	LOAD	$x30,30*REGBYTES($x1)
	LOAD	$x31,31*REGBYTES($x1)

	# restore cr29 (tid)
	LOAD	$x2,35*REGBYTES($x1)
	mtcr	$x2,ASM_CR(29)

	# gtfo!
	LOAD	$x2,33*REGBYTES($x1)
	mtpcr	$x2,ASM_CR(PCR_EPC)
	mfpcr	$x1,ASM_CR(PCR_K0)
	mfpcr	$x2,ASM_CR(PCR_K1)
	eret
	.end	pop_tf

	#define TIMER_IRQ_HANDLER \
	.align	7; \
	mtpcr	$x2,ASM_CR(PCR_K0); \
	mtpcr	$x1,ASM_CR(PCR_K1); \
	mfpcr	$x1,ASM_CR(PCR_COMPARE); \
	liw		$x2,TIMER_PERIOD; \
	addw	$x1,$x1,$x2; \
	mtpcr	$x1,ASM_CR(PCR_COMPARE); \
	mfpcr	$x1,ASM_CR(PCR_K1); \
	mfpcr	$x2,ASM_CR(PCR_K0); \
	eret

	#define TRAP_TABLE_ENTRY(x) \
	.align	7; \
	mtpcr	$ra,ASM_CR(PCR_K0); \
	mtpcr	$x1,ASM_CR(PCR_K1); \
	law		$x1,stack_top-320; \
	jal		save_tf; \
	move	$sp,$x1; \
	move	$a0,$x1; \
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
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TRAP_TABLE_ENTRY(handle_badtrap)
	TIMER_IRQ_HANDLER
	.align	12
	.end	trap_table

	.bss
	.global stack_bot
	.global stack_top
stack_bot:
	.skip	4096
stack_top:
