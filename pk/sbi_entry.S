#include "encoding.h"
#include "mcall.h"

   .section .sbi,"ax",@progbits
  .align RISCV_PGSHIFT
  .globl sbi_base
sbi_base:

  # TODO: figure out something better to do with this space.  It's not
  # protected from the OS, so beware.
  .skip RISCV_PGSIZE - 2048

  # hart_id
  .align 4
  li a0, MCALL_HART_ID
  ecall
  ret

  # num_harts
  .align 4
  lw a0, num_harts
  ret

  # query_memory
  .align 4
  j __sbi_query_memory

  # console_putchar
  .align 4
  mv a1, a0
  li a0, MCALL_CONSOLE_PUTCHAR
  ecall
  ret

  # send_device_request
  .align 4
  mv a1, a0
  li a0, MCALL_SEND_DEVICE_REQUEST
  ecall
  ret

  # receive_device_response
  .align 4
  mv a1, a0
  li a0, MCALL_RECEIVE_DEVICE_RESPONSE
  ecall
  ret

  # send ipi
  .align 4
  csrw send_ipi, a0
  ret

  # end of SBI trampolines

  .globl do_mcall
do_mcall:
  ecall
  ret

  .align RISCV_PGSHIFT
