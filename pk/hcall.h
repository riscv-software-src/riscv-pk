#ifndef _PK_HCALL_H
#define _PK_HCALL_H

#define HCALL_HART_ID 0
#define HCALL_CONSOLE_PUTCHAR 1
#define HCALL_SEND_DEVICE_REQUEST 2
#define HCALL_RECEIVE_DEVICE_RESPONSE 3

#ifndef __ASSEMBLER__

extern uintptr_t do_hcall(uintptr_t which, ...);

#endif

#endif
