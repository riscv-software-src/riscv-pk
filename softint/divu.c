
#include <stdint.h>

long 
 softint_divu( long rs1, long rs2 )
{
  // quotient = dividend / divisor + remainder
  unsigned long dividend = rs1;
  unsigned long divisor  = rs2;

  int msb = (sizeof(long) << 3) - 1;

  if (divisor == 0) return -1; 

  unsigned long temp_dividend = dividend;
  unsigned long temp_divisor = divisor;

  unsigned long quotient = 0;


  for (int i=0; i <= msb; i++)
  {
    unsigned long temp_quotient = 1;
     
    if (temp_divisor == temp_dividend)  { quotient += 1; break;}
    else if (temp_dividend < temp_divisor) { quotient += 0; break; }


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
      temp_quotient = temp_quotient << (hi - lo - 1);                
    }                                                                 
    else                                                              
    {                                                                 
      while (temp_divisor <= temp_dividend)                          
      {                                                              
        temp_divisor = temp_divisor << 1;                           
        temp_quotient = temp_quotient << 1;                         
      }                                                              
                                                                     
      temp_divisor = temp_divisor >> 1;                              
      temp_quotient = temp_quotient >> 1;                            
    }                                                                 
                                                                     
        
    temp_dividend = temp_dividend - temp_divisor;
    temp_divisor = divisor;
    quotient += temp_quotient;
  }

  return quotient;
}

