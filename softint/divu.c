
#include <stdint.h>

long 
 softint_divu( long rs1, long rs2 )
{
  // only designed to work for mabi=32
  // quotient = dividend / divisor + remainder
  unsigned long long dividend = rs1;
  unsigned long long divisor  = rs2;


  if (divisor == 0) return -1; 

  unsigned long long temp_dividend = dividend;
  unsigned long long temp_divisor = divisor;

  unsigned long long quotient = 0;


  for (int i=0; i <= 32; i++)
  {
    unsigned long long temp_quotient = 1;
     
    if (temp_divisor == temp_dividend)  { quotient += 1; break;}
    else if (temp_dividend < temp_divisor) { quotient += 0; break; }

    while (temp_divisor <= temp_dividend && temp_quotient != 0)
    {
      temp_divisor = temp_divisor << 1;
      temp_quotient = temp_quotient << 1;
    }

    temp_divisor = temp_divisor >> 1;
    temp_quotient = temp_quotient >> 1;

      
    temp_dividend = temp_dividend - temp_divisor;
    temp_divisor = divisor;
    quotient += temp_quotient;
  }

  return (long) quotient;
}

