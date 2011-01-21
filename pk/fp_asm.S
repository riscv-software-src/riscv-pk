#include "pcr.h"

	.text
	.globl	get_fp_state
	.ent	get_fp_state
get_fp_state:

	mfcr	$v0, ASM_CR(CR_FSR)

	sf.d		$f0 ,  0($a0)
	sf.d		$f1 ,  8($a0)
	sf.d		$f2 , 16($a0)
	sf.d		$f3 , 24($a0)
	sf.d		$f4 , 32($a0)
	sf.d		$f5 , 40($a0)
	sf.d		$f6 , 48($a0)
	sf.d		$f7 , 56($a0)
	sf.d		$f8 , 64($a0)
	sf.d		$f9 , 72($a0)
	sf.d		$f10, 80($a0)
	sf.d		$f11, 88($a0)
	sf.d		$f12, 96($a0)
	sf.d		$f13,104($a0)
	sf.d		$f14,112($a0)
	sf.d		$f15,120($a0)
	sf.d		$f16,128($a0)
	sf.d		$f17,136($a0)
	sf.d		$f18,144($a0)
	sf.d		$f19,152($a0)
	sf.d		$f20,160($a0)
	sf.d		$f21,168($a0)
	sf.d		$f22,176($a0)
	sf.d		$f23,184($a0)
	sf.d		$f24,192($a0)
	sf.d		$f25,200($a0)
	sf.d		$f26,208($a0)
	sf.d		$f27,216($a0)
	sf.d		$f28,224($a0)
	sf.d		$f29,232($a0)
	sf.d		$f30,240($a0)
	sf.d		$f31,248($a0)

	ret

	.end	get_fp_state

	.globl	put_fp_state
	.ent	put_fp_state
put_fp_state:

	lf.d		$f0 ,  0($a0)
	lf.d		$f1 ,  8($a0)
	lf.d		$f2 , 16($a0)
	lf.d		$f3 , 24($a0)
	lf.d		$f4 , 32($a0)
	lf.d		$f5 , 40($a0)
	lf.d		$f6 , 48($a0)
	lf.d		$f7 , 56($a0)
	lf.d		$f8 , 64($a0)
	lf.d		$f9 , 72($a0)
	lf.d		$f10, 80($a0)
	lf.d		$f11, 88($a0)
	lf.d		$f12, 96($a0)
	lf.d		$f13,104($a0)
	lf.d		$f14,112($a0)
	lf.d		$f15,120($a0)
	lf.d		$f16,128($a0)
	lf.d		$f17,136($a0)
	lf.d		$f18,144($a0)
	lf.d		$f19,152($a0)
	lf.d		$f20,160($a0)
	lf.d		$f21,168($a0)
	lf.d		$f22,176($a0)
	lf.d		$f23,184($a0)
	lf.d		$f24,192($a0)
	lf.d		$f25,200($a0)
	lf.d		$f26,208($a0)
	lf.d		$f27,216($a0)
	lf.d		$f28,224($a0)
	lf.d		$f29,232($a0)
	lf.d		$f30,240($a0)
	lf.d		$f31,248($a0)

	mtcr	$a1, ASM_CR(CR_FSR)

	ret

	.end	put_fp_state
