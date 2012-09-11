
#ifndef softint_h
#define softint_h

#ifdef __cplusplus
extern "C" {
#endif


/*----------------------------------------------------------------------------
| Integer Operations.
*----------------------------------------------------------------------------*/

long softint_mul( long, long );
long softint_udivrem( long, long, int );


#ifdef __cplusplus
}
#endif

#endif

