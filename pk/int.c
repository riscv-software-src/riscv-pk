#include "pk.h"
#include "pcr.h"


#include "softint.h"
#include "riscv-opc.h"
#include <stdint.h>

#define noisy 0


int emulate_int(trapframe_t* tf)
{
  if(noisy)
    printk("Int emulation at pc %lx, insn %x\n",tf->epc,(uint32_t)tf->insn);

  #define RS1 ((tf->insn >> 22) & 0x1F)
  #define RS2 ((tf->insn >> 17) & 0x1F)
  #define RD  ((tf->insn >> 27) & 0x1F)

//  #define XRS1 (tf->gpr[RS1])
//  #define XRS2 (tf->gpr[RS2])
  #define XRD  (tf->gpr[RD])

  unsigned long xrs1 = tf->gpr[RS1];
  unsigned long xrs2 = tf->gpr[RS2];

  #define IS_INSN(x) ((tf->insn & MASK_ ## x) == MATCH_ ## x)
   
  if(IS_INSN(DIV))
  {
    if(noisy)
      printk("emulating div\n");

    int num_negative = 0;
                     
    if ((signed long) xrs1 < 0)
    {
      xrs1 = -xrs1;
      num_negative++;
    }
                     
    if ((signed long) xrs2 < 0)
    {
      xrs2 = -xrs2;
      num_negative++;
    }

    unsigned long res = softint_divu(xrs1, xrs2);
    if (num_negative == 1)
      XRD = -res;
    else
      XRD = res;
  }
  else if(IS_INSN(DIVU))
  {
    if(noisy)
      printk("emulating divu\n");
    XRD = softint_divu( xrs1, xrs2);
  }
  else if(IS_INSN(MUL))
  {
    if(noisy)
      printk("emulating mul\n");
    XRD = softint_mul(xrs1, xrs2);
  }
  else if(IS_INSN(REM))
  {
    if(noisy)
      printk("emulating rem\n");

    if ((signed long) xrs1 < 0) {xrs1 = -xrs1;}
    if ((signed long) xrs2 < 0) {xrs2 = -xrs2;}

    XRD = softint_remu(xrs1, xrs2);
  }
  else if(IS_INSN(REMU))
  {
    if(noisy)
      printk("emulating remu\n");
    XRD = softint_remu(xrs1, xrs2);
  }
  else
    return -1;

  return 0;
}


