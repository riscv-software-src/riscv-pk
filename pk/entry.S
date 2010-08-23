	.text
	.set	noat
	.ent	save_tf
save_tf:	# write the trap frame onto the stack
	# save gprs
	sd		$1,8($k1)
	sd		$2,16($k1)
	sd		$3,24($k1)
	sd		$4,32($k1)
	sd		$5,40($k1)
	sd		$6,48($k1)
	sd		$7,56($k1)
	sd		$8,64($k1)
	sd		$9,72($k1)
	sd		$10,80($k1)
	sd		$11,88($k1)
	sd		$12,96($k1)
	sd		$13,104($k1)
	sd		$14,112($k1)
	sd		$15,120($k1)
	sd		$16,128($k1)
	sd		$17,136($k1)
	sd		$18,144($k1)
	sd		$19,152($k1)
	sd		$20,160($k1)
	sd		$21,168($k1)
	sd		$22,176($k1)
	sd		$23,184($k1)
	sd		$24,192($k1)
	sd		$25,200($k1)
	sd		$28,224($k1)
	sd		$29,232($k1)
	sd		$30,240($k1)
	sd		$k0,248($k1)	# $ra is actually in $k0
	# get sr, epc, badvaddr
	mfpcr	$t0,$0			# sr
	sd		$t0,256($k1)
	mfpcr	$t0,$1			# epc
	sd		$t0,264($k1)
	mfpcr	$t0,$2			# badvaddr
	sd		$t0,272($k1)
	jr		$ra
	.end	save_tf

	.globl	pop_tf
	.ent	pop_tf
pop_tf:	# write the trap frame onto the stack
	# restore gprs
	ld		$t0,256($a0)	# restore sr, which should disable interrupts
	mtpcr	$t0,$0

	move	$k0,$a0
	ld		$1,8($k0)
	ld		$2,16($k0)
	ld		$3,24($k0)
	ld		$4,32($k0)
	ld		$5,40($k0)
	ld		$6,48($k0)
	ld		$7,56($k0)
	ld		$8,64($k0)
	ld		$9,72($k0)
	ld		$10,80($k0)
	ld		$11,88($k0)
	ld		$12,96($k0)
	ld		$13,104($k0)
	ld		$14,112($k0)
	ld		$15,120($k0)
	ld		$16,128($k0)
	ld		$17,136($k0)
	ld		$18,144($k0)
	ld		$19,152($k0)
	ld		$20,160($k0)
	ld		$21,168($k0)
	ld		$22,176($k0)
	ld		$23,184($k0)
	ld		$24,192($k0)
	ld		$25,200($k0)
	ld		$28,224($k0)
	ld		$29,232($k0)
	ld		$30,240($k0)
	ld		$31,248($k0)

	# gtfo!
	ld		$k0,264($k0)
	mtpcr	$k0,$1
	eret
	.end	pop_tf



	#define TRAP_TABLE_ENTRY(x) \
	.align	7; \
	.set at=$k0; \
	la		$k1,stack_top-320; \
	.set noat; \
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
