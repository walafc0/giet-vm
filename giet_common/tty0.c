///////////////////////////////////////////////////////////////////////////////////
// File     : tty0.c
// Date     : 02/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The tty0.c and tty0.h files are part of the GIET-VM nano-kernel.
///////////////////////////////////////////////////////////////////////////////////

#include <hard_config.h>
#include <tty0.h>
#include <stdarg.h>
#include <tty_driver.h>
#include <utils.h>
#include <kernel_locks.h>

/////////////////////////////////////////////////////////////////////////////
// The global variable tty0_boot_mode define the type of lock used,
// and must be defined in both kernel_init.c and boot.c files.
// - the boot code must use a spin_lock because the kernel heap is not set.
// - the kernel code can use a sqt_lock when the kernel heap is set. 
/////////////////////////////////////////////////////////////////////////////

extern unsigned int  _tty0_boot_mode;

__attribute__((section(".kdata")))
sqt_lock_t           _tty0_sqt_lock  __attribute__((aligned(64)));  

__attribute__((section(".kdata")))
spin_lock_t          _tty0_spin_lock  __attribute__((aligned(64)));

//////////////////////////////////////////////
unsigned int _tty0_write( char*        buffer,
                          unsigned int nbytes )
{
    unsigned int n;
    unsigned int k;

    for ( n = 0 ; n < nbytes ; n++ ) 
    {
        // test TTY_TX buffer full 
        if ( (_tty_get_register( 0, TTY_STATUS ) & 0x2) ) // buffer full
        {
            // retry if full 
            for( k = 0 ; k < 10000 ; k++ )
            {
                if ( (_tty_get_register( 0, TTY_STATUS ) & 0x2) == 0) break;
            }
            // return error if full after 10000 retry
            return 1;
        }

        // write one byte
        if (buffer[n] == '\n') _tty_set_register( 0, TTY_WRITE, (unsigned int)'\r' );
        _tty_set_register( 0, TTY_WRITE, (unsigned int)buffer[n] );
    }
    return 0;
}

//////////////////////////
void _puts( char* string ) 
{
    unsigned int n = 0;

    while ( string[n] > 0 ) n++;

    _tty0_write( string, n );
}


//////////////////////////////
void _putx( unsigned int val )
{
    static const char HexaTab[] = "0123456789ABCDEF";
    char buf[10];
    unsigned int c;

    buf[0] = '0';
    buf[1] = 'x';

    for (c = 0; c < 8; c++) 
    { 
        buf[9 - c] = HexaTab[val & 0xF];
        val = val >> 4;
    }
    _tty0_write( buf, 10 );
}

////////////////////////////////////
void _putl( unsigned long long val )
{
    static const char HexaTab[] = "0123456789ABCDEF";
    char buf[18];
    unsigned int c;

    buf[0] = '0';
    buf[1] = 'x';

    for (c = 0; c < 16; c++) 
    { 
        buf[17 - c] = HexaTab[(unsigned int)val & 0xF];
        val = val >> 4;
    }
    _tty0_write( buf, 18 );
}

//////////////////////////////
void _putd( unsigned int val ) 
{
    static const char DecTab[] = "0123456789";
    char buf[10];
    unsigned int i;
    unsigned int first = 0;

    for (i = 0; i < 10; i++) 
    {
        if ((val != 0) || (i == 0)) 
        {
            buf[9 - i] = DecTab[val % 10];
            first = 9 - i;
        }
        else 
        {
            break;
        }
        val /= 10;
    }
    _tty0_write( &buf[first], 10 - first );
}

/////////////////////////
void _getc( char*  byte )
{
    // test status register
    while ( _tty_get_register( 0 , TTY_STATUS ) == 0 );

    // read one byte
    *byte = (char)_tty_get_register( 0 , TTY_READ );
}

//////////////////////////////////////////////////////////
static void _kernel_printf( char * format, va_list* args ) 
{

printf_text:

    while (*format) 
    {
        unsigned int i;
        for (i = 0 ; format[i] && (format[i] != '%') ; i++);
        if (i) 
        {
            if ( _tty0_write( format, i ) ) goto return_error;
            format += i;
        }
        if (*format == '%') 
        {
            format++;
            goto printf_arguments;
        }
    }

    return;

printf_arguments:

    {
        char buf[20];
        char * pbuf;
        unsigned int len = 0;
        static const char HexaTab[] = "0123456789ABCDEF";
        unsigned int i;

        switch (*format++) 
        {
            case ('c'):             /* char conversion */
            {
                int val = va_arg( *args , int );
                len = 1;
                buf[0] = val;
                pbuf = &buf[0];
                break;
            }
            case ('d'):             /* 32 bits decimal signed  */
            {
                int val = va_arg( *args , int );
                if (val < 0) 
                {
                    val = -val;
                    if ( _tty0_write( "-" , 1 ) ) goto return_error;
                }
                for(i = 0; i < 10; i++) 
                {
                    buf[9 - i] = HexaTab[val % 10];
                    if (!(val /= 10)) break;
                }
                len =  i + 1;
                pbuf = &buf[9 - i];
                break;
            }
            case ('u'):             /* 32 bits decimal unsigned  */
            {
                unsigned int val = va_arg( *args , unsigned int );
                for(i = 0; i < 10; i++) 
                {
                    buf[9 - i] = HexaTab[val % 10];
                    if (!(val /= 10)) break;
                }
                len =  i + 1;
                pbuf = &buf[9 - i];
                break;
            }
            case ('x'):             /* 32 bits hexadecimal unsigned */
            {
                unsigned int val = va_arg( *args , unsigned int );
                if ( _tty0_write( "0x" , 2 ) ) goto return_error;
                for(i = 0; i < 8; i++) 
                {
                    buf[7 - i] = HexaTab[val % 16];
                    if (!(val = (val>>4)))  break;
                }
                len =  i + 1;
                pbuf = &buf[7 - i];
                break;
            }
            case ('X'):             /* 32 bits hexadecimal unsigned  on 10 char*/
            {
                unsigned int val = va_arg( *args , unsigned int );
                if ( _tty0_write( "0x" , 2 ) ) goto return_error;
                for(i = 0; i < 8; i++) 
                {
                    buf[7 - i] = HexaTab[val % 16];
                    val = (val>>4);
                }
                len =  8;
                pbuf = buf;
                break;
            }
            case ('l'):            /* 64 bits hexadecimal unsigned */
            {
                unsigned long long val = va_arg( *args , unsigned long long );
                if ( _tty0_write( "0x" , 2 ) ) goto return_error;
                for(i = 0; i < 16; i++) 
                {
                    buf[15 - i] = HexaTab[val % 16];
                    if (!(val /= 16))  break;
                }
                len =  i + 1;
                pbuf = &buf[15 - i];
                break;
            }
            case ('s'):             /* string */
            {
                char* str = va_arg( *args , char* );
                while (str[len]) 
                {
                    len++;
                }
                pbuf = str;
                break;
            }
            default:
                goto return_error;
        }

        if ( _tty0_write( pbuf, len ) ) goto return_error;
        
        goto printf_text;
    }

return_error:

    {
        // try to print an error message and exit...
        unsigned int procid     = _get_procid();
        unsigned int x          = (procid >> (Y_WIDTH + P_WIDTH)) & ((1<<X_WIDTH)-1);
        unsigned int y          = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
        unsigned int lpid       = procid & ((1<<P_WIDTH)-1);
        _puts("\n\n[GIET ERROR] in _printf() for processor[");
        _putd( x );  
        _puts(",");
        _putd( y );
        _puts(",");
        _putd( lpid );
        _puts("]\n");

        _exit();
    }
}  // end _kernel_printf()

///////////////////////////////////////
void _nolock_printf( char* format, ...)
{
    va_list   args;

    va_start( args , format );
    _kernel_printf( format , &args );
    va_end( args );
}
////////////////////////////////
void _printf( char* format, ...)
{
    va_list       args;
    unsigned int  save_sr;

    // get TTY0 lock
    _it_disable( &save_sr );
    if ( _tty0_boot_mode ) _spin_lock_acquire( &_tty0_spin_lock );
    else                   _sqt_lock_acquire( &_tty0_sqt_lock );

    va_start( args , format );
    _kernel_printf( format , &args );
    va_end( args );

    // release TTY0 lock
    if ( _tty0_boot_mode ) _spin_lock_release( &_tty0_spin_lock );
    else                   _sqt_lock_release( &_tty0_sqt_lock );
    _it_restore( &save_sr );
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

