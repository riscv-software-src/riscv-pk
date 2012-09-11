
#include <stdint.h>


unsigned long
softint_udivrem(unsigned long num, unsigned long den, int modwanted)
{
  unsigned long bit = 1;
  unsigned long res = 0;

  while (den < num && bit && ((signed long) den >= 0))
  {
    den <<=1;
    bit <<=1;
  }
  while (bit)
  {
    if (num >= den)
    {
      num -= den;
      res |= bit;
    }
    bit >>=1;
    den >>=1;
  }
  if (modwanted) return num;
  return res;
}

