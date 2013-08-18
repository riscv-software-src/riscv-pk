// See LICENSE for license details.

#include "pcr.h"

	.text
	.globl	get_fp_state
get_fp_state:

	frsr	v0

	fsd		f0 ,  0(a0)
	fsd		f1 ,  8(a0)
	fsd		f2 , 16(a0)
	fsd		f3 , 24(a0)
	fsd		f4 , 32(a0)
	fsd		f5 , 40(a0)
	fsd		f6 , 48(a0)
	fsd		f7 , 56(a0)
	fsd		f8 , 64(a0)
	fsd		f9 , 72(a0)
	fsd		f10, 80(a0)
	fsd		f11, 88(a0)
	fsd		f12, 96(a0)
	fsd		f13,104(a0)
	fsd		f14,112(a0)
	fsd		f15,120(a0)
	fsd		f16,128(a0)
	fsd		f17,136(a0)
	fsd		f18,144(a0)
	fsd		f19,152(a0)
	fsd		f20,160(a0)
	fsd		f21,168(a0)
	fsd		f22,176(a0)
	fsd		f23,184(a0)
	fsd		f24,192(a0)
	fsd		f25,200(a0)
	fsd		f26,208(a0)
	fsd		f27,216(a0)
	fsd		f28,224(a0)
	fsd		f29,232(a0)
	fsd		f30,240(a0)
	fsd		f31,248(a0)

	ret

	.globl	put_fp_state
put_fp_state:

	fld		f0 ,  0(a0)
	fld		f1 ,  8(a0)
	fld		f2 , 16(a0)
	fld		f3 , 24(a0)
	fld		f4 , 32(a0)
	fld		f5 , 40(a0)
	fld		f6 , 48(a0)
	fld		f7 , 56(a0)
	fld		f8 , 64(a0)
	fld		f9 , 72(a0)
	fld		f10, 80(a0)
	fld		f11, 88(a0)
	fld		f12, 96(a0)
	fld		f13,104(a0)
	fld		f14,112(a0)
	fld		f15,120(a0)
	fld		f16,128(a0)
	fld		f17,136(a0)
	fld		f18,144(a0)
	fld		f19,152(a0)
	fld		f20,160(a0)
	fld		f21,168(a0)
	fld		f22,176(a0)
	fld		f23,184(a0)
	fld		f24,192(a0)
	fld		f25,200(a0)
	fld		f26,208(a0)
	fld		f27,216(a0)
	fld		f28,224(a0)
	fld		f29,232(a0)
	fld		f30,240(a0)
	fld		f31,248(a0)

	fssr	a1

	ret
