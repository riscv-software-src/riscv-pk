#ifndef _RISCV_COP0_H
#define _RISCV_COP0_H

#define mtpcr(val,reg) ({ long __tmp = (long)(val); \
                          asm volatile ("mtpcr %0,$%1"::"r"(__tmp),"i"(reg)); })

#define mfpcr(reg) ({ long __tmp; \
                      asm volatile ("mfpcr %0,$%1" : "=r"(__tmp) : "i"(reg)); \
                      __tmp; })

#endif
