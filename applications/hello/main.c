#include "stdio.h"
#include "hard_config.h"

__attribute__((constructor)) void main()
{
	char		    byte;

    // get processor identifiers
    unsigned int    x;
    unsigned int    y;
    unsigned int    lpid;
    giet_proc_xyp( &x, &y, &lpid );

    giet_tty_printf( "*** Starting task hello on processor[%d,%d,%d] at cycle %d\n\n", 
                      x, y, lpid, giet_proctime() );

	while (1)
	{
		giet_tty_printf(" hello world\n");
        giet_tty_getc((void*)&byte);
        if ( byte == 'q' ) giet_exit("quit command received");
	}
} // end main

