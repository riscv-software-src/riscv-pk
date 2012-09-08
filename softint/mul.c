 
#include <stdint.h>

long softint_mul( long x, long y )
{

  int i;
  long result = 0;

  for (i = 0; i < sizeof(long); i++) {
    if ((x & 0x1) == 1)
      result = result + y;

    x = x >> 1;
    y = y << 1;
  }

  return result;

}

 
