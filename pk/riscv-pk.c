// force the linker to pull in our __start from boot.S.
void* dummy()
{
  extern void __start();
  return &__start;
}
