	.text
	.global __start
	.ent    __start
__start:

	# set up SR
	li		$t0, 0xFF
	mtpcr	$t0, $0

	# set up trap table
	la		$t0, trap_table
	mtpcr	$t0, $3

	# set up stack
	la		$sp,stack_top-64

	# call into kernel
	la		$t0, boot
	jr		$t0

	.end    __start
