#include "mimpl.h"
#include "target.h"

void mimpl_init(void)
{
#error "Put initialization code for your machine target."
}

static void target_putchar(uint8_t ch)
{
#error "Put the code that output a character to debug console."
}

static uintptr_t target_getchar()
{
#error "Put the code that return a character from debug console."
  return 0;
}

static void __attribute((noreturn)) target_poweroff()
{
#error "Put power-off code for your machine target."
  while (1);
}

static int target_swint_pending()
{
#error "Put the code that check software interrupt pending condition for your machine target."
  return 0;
}

static void target_timer_callback()
{
#error "Put the code that will be executed on timer interrupt "
}

struct mimpl_ops target_ops = {
  .console_putchar	= target_putchar,
  .console_getchar	= target_getchar,
  .power_off		= target_poweroff,
  .swint_pending	= target_swint_pending,
  .timer_callback	= target_timer_callback,
};

struct mimpl_ops* mimpl_ops(void)
{
  return &target_ops;
}
