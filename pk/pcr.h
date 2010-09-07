#ifndef _RISCV_COP0_H
#define _RISCV_COP0_H

#include "config.h"

#define SR_ET    0x0000000000000001
#define SR_PS    0x0000000000000004
#define SR_S     0x0000000000000008
#define SR_EF    0x0000000000000010
#define SR_UX    0x0000000000000020
#define SR_KX    0x0000000000000040
#define SR_IM    0x000000000000FF00

#define PCR_SR       0
#define PCR_EPC      1
#define PCR_BADVADDR 2
#define PCR_TBR      3
#define PCR_TOHOST   16
#define PCR_FROMHOST 17
#define PCR_K0       24
#define PCR_K1       25

#define ASM_CR(r)   _ASM_CR(r)
#define _ASM_CR(r)  $cr##r

#ifndef __ASSEMBLER__

#define mtpcr(val,reg) ({ long __tmp = (long)(val); \
          asm volatile ("mtpcr %0,$cr%1"::"r"(__tmp),"i"(reg)); })

#define mfpcr(reg) ({ long __tmp; \
          asm volatile ("mfpcr %0,$cr%1" : "=r"(__tmp) : "i"(reg)); \
          __tmp; })

#endif

#endif
