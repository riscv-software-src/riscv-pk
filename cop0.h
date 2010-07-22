#ifndef _RISCV_COP0_H
#define _RISCV_COP0_H

#define dmtc0(val,reg) ({ long __tmp = (long)(val); \
                          asm volatile ("dmtc0 %0,$%1"::"r"(__tmp),"i"(reg)); })

#define dmfc0(reg) ({ long __tmp; \
                      asm volatile ("dmfc0 %0,$%1" : "=r"(__tmp) : "i"(reg)); \
                      __tmp; })

#endif
