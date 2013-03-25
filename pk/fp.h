// See LICENSE for license details.

#ifndef _FP_H
#define _FP_H

typedef struct
{
  uint64_t fpr[32];
  uint32_t fsr;
} fp_state_t;

void put_fp_state(const void* fp_regs, long fsr);
long get_fp_state(void* fp_regs);

#endif
