#include "encoding.h"
#include "hcall.h"

   .section .sbi,"ax",@progbits
  .align RISCV_PGSHIFT
  .globl sbi_base
sbi_base:

  # TODO: figure out something better to do with this space.  It's not
  # protected from the OS, so beware.
  .skip RISCV_PGSIZE - 2048

  # hart_id
  .align 4
  li a0, HCALL_HART_ID
  hcall
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
  li a0, HCALL_CONSOLE_PUTCHAR
  hcall
  ret

  # send_device_request
  .align 4
  mv a1, a0
  li a0, HCALL_SEND_DEVICE_REQUEST
  hcall
  ret

  # receive_device_response
  .align 4
  mv a1, a0
  li a0, HCALL_RECEIVE_DEVICE_RESPONSE
  hcall
  ret

  # send ipi
  .align 4
  csrw send_ipi, a0
  ret

  # end of SBI trampolines

  .globl do_hcall
do_hcall:
  hcall
  ret

  .align RISCV_PGSHIFT
