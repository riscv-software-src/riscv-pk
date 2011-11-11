#ifndef _RISCV_COP0_H
#define _RISCV_COP0_H

#define SR_ET    0x0000000000000001
#define SR_EF    0x0000000000000002
#define SR_EV    0x0000000000000004
#define SR_EC    0x0000000000000008
#define SR_PS    0x0000000000000010
#define SR_S     0x0000000000000020
#define SR_UX    0x0000000000000040
#define SR_SX    0x0000000000000080
#define SR_IM    0x000000000000FF00
#define SR_VM    0x0000000000010000

#define SR_IM_SHIFT 8

#define PCR_SR       0
#define PCR_EPC      1
#define PCR_BADVADDR 2
#define PCR_EVEC     3
#define PCR_COUNT    4
#define PCR_COMPARE  5
#define PCR_CAUSE    6
#define PCR_PTBR     7
#define PCR_SEND_IPI 8
#define PCR_CLR_IPI  9
#define PCR_COREID   10
#define PCR_K0       12
#define PCR_K1       13
#define PCR_TOHOST   16
#define PCR_FROMHOST 17
#define PCR_CONSOLE  18

#define IPI_IRQ   5
#define TIMER_IRQ 7

#define CAUSE_MISALIGNED_FETCH 0
#define CAUSE_FAULT_FETCH 1
#define CAUSE_ILLEGAL_INSTRUCTION 2
#define CAUSE_PRIVILEGED_INSTRUCTION 3
#define CAUSE_FP_DISABLED 4
#define CAUSE_SYSCALL 6
#define CAUSE_BREAKPOINT 7
#define CAUSE_MISALIGNED_LOAD 8
#define CAUSE_MISALIGNED_STORE 9
#define CAUSE_FAULT_LOAD 10
#define CAUSE_FAULT_STORE 11
#define CAUSE_VECTOR_DISABLED 12
#define CAUSE_VECTOR_BANK 13
#define CAUSE_VECTOR_ILLEGAL_INSTRUCTION 14
#define CAUSE_IRQ0 16
#define CAUSE_IRQ1 17
#define CAUSE_IRQ2 18
#define CAUSE_IRQ3 19
#define CAUSE_IRQ4 20
#define CAUSE_IRQ5 21
#define CAUSE_IRQ6 22
#define CAUSE_IRQ7 23
#define NUM_CAUSES 24

#define ASM_CR(r)   _ASM_CR(r)
#define _ASM_CR(r)  cr##r

#ifndef __ASSEMBLER__

#define mtpcr(reg,val) ({ long __tmp = (long)(val); \
          asm volatile ("mtpcr %0,cr%1"::"r"(__tmp),"i"(reg)); })

#define mfpcr(reg) ({ long __tmp; \
          asm volatile ("mfpcr %0,cr%1" : "=r"(__tmp) : "i"(reg)); \
          __tmp; })

#define irq_disable() asm volatile("di")
#define irq_enable() asm volatile("ei")

#endif

#endif
