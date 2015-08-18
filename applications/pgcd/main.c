#include "stdio.h"
#include "hard_config.h"

/////////////////////////////////////////
__attribute__ ((constructor)) void main()
{
    unsigned int opx;
    unsigned int opy;

    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    lpid;
    giet_proc_xyp( &x, &y, &lpid );

    giet_tty_printf( "*** Starting task pgcd on processor[%d,%d,%d] at cycle %d\n\n", 
                      x, y, lpid, giet_proctime() );

    while (1) 
    {
        giet_tty_printf("\n*******************\n");
        giet_tty_printf("operand X = ");
        giet_tty_getw( &opx );
        giet_tty_printf("\n");
        giet_tty_printf("operand Y = ");
        giet_tty_getw( &opy );
        giet_tty_printf("\n");
        if( (opx == 0) || (opy == 0) ) 
        {
           giet_tty_printf("operands must be larger than 0\n");
        } 
        else 
        {
            while (opx != opy) 
            {
                if(opx > opy)   opx = opx - opy;
                else            opy = opy - opx;
            }
            giet_tty_printf("pgcd      = %d\n", opx);
        }
    }
} // end pgcd

