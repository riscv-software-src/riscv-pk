	.text
	.global __start
	.ent    __start
__start:

	# set up SR
	li		$t0, 0xFF
	dmtc0	$t0, $0

	# set up trap table
	dla		$t0, trap_table
	dmtc0	$t0, $3

	# set up stack
	dla		$sp,stack_top-64

	# call into kernel
	dla		$t0, boot
	jr		$t0

	.end    __start
