
#include <stdint.h>

long 
 softint_remu( long rs1, long rs2 )
{
  // quotient = dividend / divisor + remainder
  unsigned long dividend = rs1;
  unsigned long divisor  = rs2;
  
  int msb = (sizeof(long) << 3) - 1;

  if (divisor == 0) { return dividend; }

  unsigned long temp_dividend = dividend;
  unsigned long temp_divisor = divisor;

  for (int i=0; i <= msb; i++)
  {
    if (temp_divisor == temp_dividend) { return 0; }
    else if (temp_dividend < temp_divisor) { return temp_dividend; }

 
    // check for corner-case when (msb of dividend)==1                              
    if ((temp_dividend & (1 << msb)) == (1 << msb))                     
    {                                                                 
      int hi = msb, lo = 0;                                           
      for (int i=msb; i >= 0; i--)                                     
      {                                                              
        unsigned int mask = 1 << i;                                 
        if ((mask & temp_divisor) == mask)                          
        {                                                           
          lo = i;                                                  
          break;                                                   
        }                                                           
      }                                                              
      temp_divisor = temp_divisor << (hi - lo - 1);                  
    }                                                                 
    else                                                              
    {                                                                 
      while ( (unsigned long) temp_divisor <= (unsigned long) temp_dividend)                          
      {                                                              
        temp_divisor = temp_divisor << 1;                           
      }                                                              
                                                                     
      temp_divisor = temp_divisor >> 1;                              
    }


    temp_dividend = temp_dividend - temp_divisor;
    temp_divisor = divisor;
  }

  return temp_dividend;
}

