#include "pcr.h"

	.text
	.globl	get_fp_state
	.ent	get_fp_state
get_fp_state:

	mfcr	$v0, ASM_CR(CR_FSR)

	s.d		$f0 ,  0($a0)
	s.d		$f1 ,  8($a0)
	s.d		$f2 , 16($a0)
	s.d		$f3 , 24($a0)
	s.d		$f4 , 32($a0)
	s.d		$f5 , 40($a0)
	s.d		$f6 , 48($a0)
	s.d		$f7 , 56($a0)
	s.d		$f8 , 64($a0)
	s.d		$f9 , 72($a0)
	s.d		$f10, 80($a0)
	s.d		$f11, 88($a0)
	s.d		$f12, 96($a0)
	s.d		$f13,104($a0)
	s.d		$f14,112($a0)
	s.d		$f15,120($a0)
	s.d		$f16,128($a0)
	s.d		$f17,136($a0)
	s.d		$f18,144($a0)
	s.d		$f19,152($a0)
	s.d		$f20,160($a0)
	s.d		$f21,168($a0)
	s.d		$f22,176($a0)
	s.d		$f23,184($a0)
	s.d		$f24,192($a0)
	s.d		$f25,200($a0)
	s.d		$f26,208($a0)
	s.d		$f27,216($a0)
	s.d		$f28,224($a0)
	s.d		$f29,232($a0)
	s.d		$f30,240($a0)
	s.d		$f31,248($a0)

	.end	get_fp_state

	.globl	put_fp_state
	.ent	put_fp_state
put_fp_state:

	l.d		$f0 ,  0($a0)
	l.d		$f1 ,  8($a0)
	l.d		$f2 , 16($a0)
	l.d		$f3 , 24($a0)
	l.d		$f4 , 32($a0)
	l.d		$f5 , 40($a0)
	l.d		$f6 , 48($a0)
	l.d		$f7 , 56($a0)
	l.d		$f8 , 64($a0)
	l.d		$f9 , 72($a0)
	l.d		$f10, 80($a0)
	l.d		$f11, 88($a0)
	l.d		$f12, 96($a0)
	l.d		$f13,104($a0)
	l.d		$f14,112($a0)
	l.d		$f15,120($a0)
	l.d		$f16,128($a0)
	l.d		$f17,136($a0)
	l.d		$f18,144($a0)
	l.d		$f19,152($a0)
	l.d		$f20,160($a0)
	l.d		$f21,168($a0)
	l.d		$f22,176($a0)
	l.d		$f23,184($a0)
	l.d		$f24,192($a0)
	l.d		$f25,200($a0)
	l.d		$f26,208($a0)
	l.d		$f27,216($a0)
	l.d		$f28,224($a0)
	l.d		$f29,232($a0)
	l.d		$f30,240($a0)
	l.d		$f31,248($a0)

	mtcr	$a1, ASM_CR(CR_FSR)

	.end	put_fp_state
