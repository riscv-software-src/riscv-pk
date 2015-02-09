// See LICENSE for license details.

#ifndef _FP_H
#define _FP_H

typedef union {
  struct {
    uint8_t flags : 5;
    uint8_t rm : 3;
  } fcsr;
  uint8_t bits;
} fcsr_t;

typedef struct
{
  uint64_t fpr[32];
  fcsr_t fcsr;
} fp_state_t;

void put_fp_state(const void* fp_regs, uint8_t fsr);
long get_fp_state(void* fp_regs);

#endif
