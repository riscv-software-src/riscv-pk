
#include <stdint.h>

long 
 softint_remu( long rs1, long rs2 )
{
  // only designed to work for mabi=32
  // quotient = dividend / divisor + remainder

  unsigned long long dividend = rs1;
  unsigned long long divisor  = rs2;

  if (divisor == 0) { return dividend; }

  long long temp_dividend = dividend;
  long long temp_divisor = divisor;

  for (int i=0; i <= 32; i++)
  {
    if (temp_divisor == temp_dividend) { return 0; }
    else if (temp_dividend < temp_divisor) { return (long) temp_dividend; }


    while (temp_divisor <= temp_dividend && temp_divisor != 0)
    {
      temp_divisor = temp_divisor << 1;
    }

    temp_divisor = temp_divisor >> 1;


    temp_dividend = temp_dividend - temp_divisor;
    temp_divisor = divisor;
  }

  return (long) temp_dividend;
}

