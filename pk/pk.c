#include "pk.h"
#include "mmap.h"
#include "boot.h"
#include "elf.h"
#include "mtrap.h"
#include "frontend.h"
#include <stdbool.h>

elf_info current;

#define NUM_COUNTERS 18
static int uarch_counters_enabled;
static long uarch_counters[NUM_COUNTERS];
static char* uarch_counter_names[NUM_COUNTERS];

static void read_uarch_counters(bool dump)
{
  if (!uarch_counters_enabled)
    return;

  size_t i = 0;
  #define READ_CTR(name) do { \
    while (i >= NUM_COUNTERS) ; \
    long csr = read_csr(name); \
    if (dump && csr) printk("%s = %ld\n", #name, csr - uarch_counters[i]); \
    uarch_counters[i++] = csr; \
  } while (0)
  READ_CTR(0xcc0); READ_CTR(0xcc1); READ_CTR(0xcc2);
  READ_CTR(0xcc3); READ_CTR(0xcc4); READ_CTR(0xcc5);
  READ_CTR(0xcc6); READ_CTR(0xcc7); READ_CTR(0xcc8);
  READ_CTR(0xcc9); READ_CTR(0xcca); READ_CTR(0xccb);
  READ_CTR(0xccc); READ_CTR(0xccd); READ_CTR(0xcce);
  READ_CTR(0xccf); READ_CTR(cycle); READ_CTR(instret);
  #undef READ_CTR
}

static void start_uarch_counters()
{
  read_uarch_counters(false);
}

void dump_uarch_counters()
{
  read_uarch_counters(true);
}

static void handle_option(const char* s)
{
  switch (s[1])
  {
    case 's': // print cycle count upon termination
      current.t0 = 1;
      break;

    case 'c': // print uarch counters upon termination
      uarch_counters_enabled = 1;
      break;

    default:
      panic("unrecognized option: `%c'", s[1]);
      break;
  }
}

#define MAX_ARGS 64
typedef union {
  uint64_t buf[MAX_ARGS];
  char* argv[MAX_ARGS];
} arg_buf;

static size_t parse_args(arg_buf* args)
{
  long r = frontend_syscall(SYS_getmainvars, va2pa(args), sizeof(*args), 0, 0, 0, 0, 0);
  kassert(r == 0);
  uint64_t* pk_argv = &args->buf[1];
  // pk_argv[0] is the proxy kernel itself.  skip it and any flags.
  size_t pk_argc = args->buf[0], arg = 1;
  for ( ; arg < pk_argc && *(char*)(uintptr_t)pk_argv[arg] == '-'; arg++)
    handle_option((const char*)(uintptr_t)pk_argv[arg]);

  for (size_t i = 0; arg + i < pk_argc; i++)
    args->argv[i] = (char*)(uintptr_t)pk_argv[arg + i];
  return pk_argc - arg;
}

static void init_tf(trapframe_t* tf, long pc, long sp)
{
  memset(tf, 0, sizeof(*tf));
  tf->status = (read_csr(sstatus) &~ SSTATUS_SPP &~ SSTATUS_SIE) | SSTATUS_SPIE;
  tf->gpr[2] = sp;
  tf->epc = pc;
}

static void run_loaded_program(size_t argc, char** argv, uintptr_t kstack_top)
{
  // copy phdrs to user stack
  size_t stack_top = current.stack_top - current.phdr_size;
  memcpy((void*)stack_top, (void*)current.phdr, current.phdr_size);
  current.phdr = stack_top;

  // copy argv to user stack
  for (size_t i = 0; i < argc; i++) {
    size_t len = strlen((char*)(uintptr_t)argv[i])+1;
    stack_top -= len;
    memcpy((void*)stack_top, (void*)(uintptr_t)argv[i], len);
    argv[i] = (void*)stack_top;
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
    *((type*)sp) = (type)value; \
    sp += sizeof(type); \
  } while (0)

  #define STACK_INIT(type) do { \
    unsigned naux = sizeof(aux)/sizeof(aux[0]); \
    stack_top -= (1 + argc + 1 + 1 + 2*naux) * sizeof(type); \
    stack_top &= -16; \
    long sp = stack_top; \
    PUSH_ARG(type, argc); \
    for (unsigned i = 0; i < argc; i++) \
      PUSH_ARG(type, argv[i]); \
    PUSH_ARG(type, 0); /* argv[argc] = NULL */ \
    PUSH_ARG(type, 0); /* envp[0] = NULL */ \
    for (unsigned i = 0; i < naux; i++) { \
      PUSH_ARG(type, aux[i].key); \
      PUSH_ARG(type, aux[i].value); \
    } \
  } while (0)

  STACK_INIT(uintptr_t);

  if (current.t0) // start timer if so requested
    current.t0 = rdcycle();

  start_uarch_counters();

  trapframe_t tf;
  init_tf(&tf, current.entry, stack_top);
  __clear_cache(0, 0);
  write_csr(sscratch, kstack_top);
  start_user(&tf);
}

static void rest_of_boot_loader(uintptr_t kstack_top)
{
  arg_buf args;
  size_t argc = parse_args(&args);
  if (!argc)
    panic("tell me what ELF to load!");

  // load program named by argv[0]
  long phdrs[128];
  current.phdr = (uintptr_t)phdrs;
  current.phdr_size = sizeof(phdrs);
  load_elf(args.argv[0], &current);

  run_loaded_program(argc, args.argv, kstack_top);
}

void boot_loader()
{
  extern char trap_entry;
  write_csr(stvec, &trap_entry);
  write_csr(sscratch, 0);
  write_csr(sie, 0);

  file_init();
  enter_supervisor_mode(rest_of_boot_loader, pk_vm_init());
}

void boot_other_hart()
{
  // stall all harts besides hart 0
  while (1)
    wfi();
}
