#ifndef _ASM_RISCV_SBI_H
#define _ASM_RISCV_SBI_H

typedef struct {
  unsigned long base;
  unsigned long size;
  unsigned long node_id;
} memory_block_info;

unsigned long sbi_query_memory(unsigned long id, memory_block_info *p);

unsigned long sbi_hart_id(void);
unsigned long sbi_num_harts(void);
void sbi_send_ipi(uintptr_t hart_id);
void sbi_console_putchar(unsigned char ch);

typedef struct {
  unsigned long dev;
  unsigned long cmd;
  unsigned long data;
  unsigned long sbi_private_data;
} sbi_device_message;

unsigned long sbi_send_device_request(uintptr_t req);
uintptr_t sbi_receive_device_response(void);

#endif
