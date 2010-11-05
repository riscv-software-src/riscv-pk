#include <string.h>
#include <stdlib.h>

// from http://www-graphics.stanford.edu/~seander/bithacks.html
static inline long hasZeroByte(long l)
{
  if(sizeof(long) == 4)
    return (l - 0x01010101UL) & ~l & 0x80808080UL;
  else if(sizeof(long) == 8)
    return (l - 0x0101010101010101UL) & ~l & 0x8080808080808080UL;
}

size_t strlen(const char* s)
{
  size_t i = 0;

  // use optimized version if string starts on a long boundary
  if(((long)s & (sizeof(long)-1)) == 0)
    while(!hasZeroByte(*(long*)(s+i)))
      i += sizeof(long);

  while(s[i])
    i++;

  return i;
}
