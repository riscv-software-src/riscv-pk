#include "pk.h"
#include "vm.h"
#include "elf.h"

void run_loaded_program(struct mainvars* args)
{
  if (current.is_supervisor)
    panic("pk can't run kernel binaries; try using bbl instead");

  uintptr_t kernel_stack_top = pk_vm_init();

  extern char trap_entry;
  write_csr(stvec, &trap_entry);
  write_csr(sscratch, 0);

  // enter supervisor mode
  asm volatile("la t0, 1f; csrw mepc, t0; eret; 1:" ::: "t0");

  // copy phdrs to user stack
  size_t stack_top = current.stack_top - current.phdr_size;
  memcpy((void*)stack_top, (void*)current.phdr, current.phdr_size);
  current.phdr = stack_top;

  // copy argv to user stack
  for (size_t i = 0; i < args->argc; i++) {
    size_t len = strlen((char*)(uintptr_t)args->argv[i])+1;
    stack_top -= len;
    memcpy((void*)stack_top, (void*)(uintptr_t)args->argv[i], len);
    args->argv[i] = stack_top;
  }
  stack_top &= -sizeof(void*);

  struct {
    long key;
    long value;
  } aux[] = {
    {AT_ENTRY, current.entry},
    {AT_PHNUM, current.phnum},
    {AT_PHENT, current.phent},
    {AT_PHDR, current.phdr},
    {AT_PAGESZ, RISCV_PGSIZE},
    {AT_SECURE, 0},
    {AT_RANDOM, stack_top},
    {AT_NULL, 0}
  };

  // place argc, argv, envp, auxp on stack
  #define PUSH_ARG(type, value) do { \
    *((type*)sp) = value; \
    sp += sizeof(type); \
  } while (0)

  #define STACK_INIT(type) do { \
    unsigned naux = sizeof(aux)/sizeof(aux[0]); \
    stack_top -= (1 + args->argc + 1 + 1 + 2*naux) * sizeof(type); \
    stack_top &= -16; \
    long sp = stack_top; \
    PUSH_ARG(type, args->argc); \
    for (unsigned i = 0; i < args->argc; i++) \
      PUSH_ARG(type, args->argv[i]); \
    PUSH_ARG(type, 0); /* argv[argc] = NULL */ \
    PUSH_ARG(type, 0); /* envp[0] = NULL */ \
    for (unsigned i = 0; i < naux; i++) { \
      PUSH_ARG(type, aux[i].key); \
      PUSH_ARG(type, aux[i].value); \
    } \
  } while (0)

  if (current.elf64)
    STACK_INIT(uint64_t);
  else
    STACK_INIT(uint32_t);

  if (current.t0) // start timer if so requested
    current.t0 = rdcycle();

  if (uarch_counters_enabled) { // start tracking the uarch counters if requested
    size_t i = 0;
    #define READ_CTR_INIT(name) do { \
      while (i >= NUM_COUNTERS) ; \
      long csr = read_csr(name); \
      uarch_counters[i++] = csr; \
    } while (0)
    READ_CTR_INIT(cycle);   READ_CTR_INIT(instret);
    READ_CTR_INIT(uarch0);  READ_CTR_INIT(uarch1);  READ_CTR_INIT(uarch2);
    READ_CTR_INIT(uarch3);  READ_CTR_INIT(uarch4);  READ_CTR_INIT(uarch5);
    READ_CTR_INIT(uarch6);  READ_CTR_INIT(uarch7);  READ_CTR_INIT(uarch8);
    READ_CTR_INIT(uarch9);  READ_CTR_INIT(uarch10); READ_CTR_INIT(uarch11);
    READ_CTR_INIT(uarch12); READ_CTR_INIT(uarch13); READ_CTR_INIT(uarch14);
    READ_CTR_INIT(uarch15);
    #undef READ_CTR_INIT
  }

  trapframe_t tf;
  init_tf(&tf, current.entry, stack_top, current.elf64);
  __clear_cache(0, 0);
  write_csr(sscratch, kernel_stack_top);
  start_user(&tf);
}

void boot_other_hart()
{
  // stall all harts besides hart 0
  while (1)
    wfi();
}
