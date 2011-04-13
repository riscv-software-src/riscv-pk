#include <limits.h>
#include <string.h>

#if ULONG_MAX != 18446744073709551615UL && ULONG_MAX != 4294967295UL
# error need sizeof(long) == 4 or sizeof(long) == 8
#endif

void* memset(void* m, int ch, size_t s)
{
  size_t i = 0;
  char* mem = (char*)m;
  long* lmem = (long*)mem;

  long l = ch & 0xFF;
  l = l | (l << 8);
  l = l | (l << 16);
  #if ULONG_MAX == 18446744073709551615UL
  l = l | (l << 32);
  #endif

  if(s < 8*sizeof(long))
    goto small;

  if((long)m & (sizeof(long)-1))
  {
    size_t n = sizeof(long) - ((long)m & (sizeof(long)-1));
    for(i = 0; i < n; i++)
      mem[i] = ch;

    mem += n;
    s -= n;
  }

  for(i = 0; i < s/sizeof(long) - 7; i += 8)
  {
    lmem[i+0] = l;
    lmem[i+1] = l;
    lmem[i+2] = l;
    lmem[i+3] = l;
    lmem[i+4] = l;
    lmem[i+5] = l;
    lmem[i+6] = l;
    lmem[i+7] = l;
  }

small:
  for( ; i < s/sizeof(long); i++)
    lmem[i] = l;

  for(i *= sizeof(long); i < s; i++)
    mem[i] = ch;

  return m;
}
