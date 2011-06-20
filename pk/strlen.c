#include <string.h>
#include <limits.h>

#if ULONG_MAX != 18446744073709551615UL && ULONG_MAX != 4294967295UL
# error need sizeof(long) == 4 or sizeof(long) == 8
#endif

// from http://www-graphics.stanford.edu/~seander/bithacks.html
static inline long hasZeroByte(long l)
{
#if ULONG_MAX == 4294967295UL
  return (l - 0x01010101UL) & ~l & 0x80808080UL;
#else
  return (l - 0x0101010101010101UL) & ~l & 0x8080808080808080UL;
#endif
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
