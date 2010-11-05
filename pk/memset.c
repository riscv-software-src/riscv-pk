#include <stdlib.h>
#include <limits.h>
#include <string.h>

void* memset(void* m, int ch, size_t s)
{
  char* mem = (char*)m;
  while(((long)m & (sizeof(long)-1)) && s)
  {
    *mem++ = ch;
    s--;
  }

  long l = ch & 0xFF;
  l = l | (l << 8);
  l = l | (l << 16);
  if(sizeof(long) == 8)
    l = l | (l << 32);
  else if(sizeof(long) != 4)
    abort();

  long* lmem = (long*)mem;
  for(size_t i = 0; i < (s+sizeof(long)-1)/sizeof(long)*sizeof(long); i += 8)
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

  for(size_t i = (s+sizeof(long)-1)/sizeof(long)*sizeof(long); i < s; i++)
    mem[i] = ch;

  return m;
}
