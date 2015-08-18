//////////////////////////////////////////////////////////////////////////////
// File     : stdio.c         
// Date     : 01/04/2010
// Author   : alain greiner & Joel Porquet
// Copyright (c) UPMC-LIP6
//////////////////////////////////////////////////////////////////////////////

#include <stdarg.h>
#include <stdio.h>
#include <giet_config.h>

//////////////////////////////////////////////////////////////////////////////
/////////////////////  MIPS32     related system calls ///////////////////////
//////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////
void giet_proc_xyp( unsigned int* cluster_x,
                    unsigned int* cluster_y,
                    unsigned int* lpid )
{
    sys_call( SYSCALL_PROC_XYP,
              (unsigned int)cluster_x,
              (unsigned int)cluster_y,
              (unsigned int)lpid,
               0 );
}

////////////////////////////
unsigned int giet_proctime() 
{
    return (unsigned int)sys_call( SYSCALL_PROC_TIME, 
                                   0, 0, 0, 0 );
}

////////////////////////
unsigned int giet_rand() 
{
    unsigned int x = (unsigned int)sys_call( SYSCALL_PROC_TIME,
                                             0, 0, 0, 0);
    if ((x & 0xF) > 7) 
    {
        return (x*x & 0xFFFF);
    }
    else 
    {
        return (x*x*x & 0xFFFF);
    }
}

//////////////////////////////////////////////////////////////////////////////
///////////////////// Task related  system calls /////////////////////////////
//////////////////////////////////////////////////////////////////////////////

////////////////////////////////
unsigned int giet_proc_task_id() 
{
    return (unsigned int)sys_call( SYSCALL_LOCAL_TASK_ID, 
                                   0, 0, 0, 0 );
}

//////////////////////////////////
unsigned int giet_global_task_id() 
{
    return (unsigned int)sys_call( SYSCALL_GLOBAL_TASK_ID, 
                                   0, 0, 0, 0 );
}

/////////////////////////////
unsigned int giet_thread_id() 
{
    return (unsigned int)sys_call( SYSCALL_THREAD_ID, 
                                   0, 0, 0, 0 );
}

//////////////////////////////
void giet_exit( char* string ) 
{
    sys_call( SYSCALL_EXIT,
              (unsigned int)string,
              0, 0, 0 );
}

/////////////////////////////////////////
void giet_assert( unsigned int condition,
                  char*        string )
{
    if ( condition == 0 ) giet_exit( string );
}

//////////////////////////
void giet_context_switch() 
{
    sys_call( SYSCALL_CTX_SWITCH,
              0, 0, 0, 0 );
}

////////////////////////
void giet_tasks_status()
{
    sys_call( SYSCALL_TASKS_STATUS,
              0, 0, 0, 0 );
}

//////////////////////////////////////////////////////////////////////////////
///////////////////// Applications  system calls /////////////////////////////
//////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
int giet_kill_application( char* name ) 
{
    return ( sys_call( SYSCALL_KILL_APP,
                       (unsigned int)name,
                       0, 0, 0 ) );
}

///////////////////////////////////////
int giet_exec_application( char* name ) 
{
    return ( sys_call( SYSCALL_EXEC_APP,
                       (unsigned int)name,
                       0, 0, 0 ) );
}

//////////////////////////////////////////////////////////////////////////////
///////////////////// Coprocessors  system calls  ////////////////////////////
//////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////
void giet_coproc_alloc( unsigned int   coproc_type,
                        unsigned int*  coproc_info )
{
    if ( sys_call( SYSCALL_COPROC_ALLOC,
                   coproc_type,
                   (unsigned int)coproc_info,
                   0, 0 ) )  
        giet_exit("error in giet_coproc_alloc()");
}

/////////////////////////////////////////////////////////
void giet_coproc_release( unsigned int coproc_reg_index )
{
    if ( sys_call( SYSCALL_COPROC_RELEASE,
                   coproc_reg_index,
                   0, 0, 0 ) )  
        giet_exit("error in giet_coproc_release()");
}

//////////////////////////////////////////////////////////////////
void giet_coproc_channel_init( unsigned int            channel,
                               giet_coproc_channel_t*  desc )
{
    if ( sys_call( SYSCALL_COPROC_CHANNEL_INIT,
                   channel,
                   (unsigned int)desc,
                   0, 0 ) ) 
        giet_exit("error in giet_coproc_channel_init()");
}

/////////////////////////////////////////////////////
void giet_coproc_run( unsigned int coproc_reg_index )
{
    if ( sys_call( SYSCALL_COPROC_RUN,
                   coproc_reg_index,
                   0, 0, 0 ) ) 
        giet_exit("error in giet_coproc_run()");
}

////////////////////////////
void giet_coproc_completed()
{
    if ( sys_call( SYSCALL_COPROC_COMPLETED,
                   0, 0, 0, 0 ) ) 
        giet_exit("error in giet_coproc_completed");
}


//////////////////////////////////////////////////////////////////////////////
/////////////////////  TTY device related system calls ///////////////////////
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
void giet_tty_alloc( unsigned int shared )
{
    if ( sys_call( SYSCALL_TTY_ALLOC,
                   shared,
                   0, 0, 0 ) )  giet_exit("error in giet_tty_alloc()");
}

////////////////////////////////////////////////////////////////////////
static  int __printf( char* format, unsigned int channel, va_list* args) 
{
    int ret;                    // return value from the syscall 
    enum TModifiers {NO_MOD, L_MOD, LL_MOD} modifiers;

printf_text:

    while (*format) 
    {
        unsigned int i;
        for (i = 0 ; format[i] && (format[i] != '%') ; i++);
        if (i) 
        {
            ret = sys_call(SYSCALL_TTY_WRITE, 
                           (unsigned int)format,
                           i, 
                           channel,
                           0);

            if (ret != i) goto return_error;

            format += i;
        }
        if (*format == '%') 
        {
            format++;
            modifiers = NO_MOD;
            goto printf_arguments;
        }
    }

    return 0;

printf_arguments:

    {
        char              buf[30];
        char *            pbuf;
        unsigned int      len = 0;
        static const char HexaTab[] = "0123456789ABCDEF";
        unsigned int      i;
        
        /* Ignored fields : width and precision */
        for (; *format >= '0' && *format <= '9'; format++);

        switch (*format++) 
        {
            case ('%'):
            {
                len = 1;
                pbuf = "%";
                break;
            }
            case ('c'):             /* char conversion */
            {
                int val = va_arg( *args, int );
                if (modifiers != NO_MOD) goto return_error; // Modifiers have no meaning
                
                len = 1;
                buf[0] = val;
                pbuf = &buf[0];
                break;
            }
            case ('d'):             /* decimal signed integer */
            {
                int val = va_arg( *args, int );
                
                if (modifiers == LL_MOD) goto return_error; // 64 bits not supported
                
                if (val < 0) 
                {
                    val = -val;
                    ret = sys_call(SYSCALL_TTY_WRITE, 
                                   (unsigned int)"-",
                                   1,
                                   channel,
                                   0);
                    if (ret != 1) goto return_error;
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
            case ('u'):             /* decimal unsigned integer */
            {
                if (modifiers != LL_MOD) //32 bits integer
                {
                    unsigned int val = va_arg( *args, unsigned int );
                    for(i = 0; i < 10; i++) 
                    {
                        buf[9 - i] = HexaTab[val % 10];
                        if (!(val /= 10)) break;
                    }
                    len =  i + 1;
                    pbuf = &buf[9 - i];
                    break;
                }
                //64 bits : base 10 unsupported : continue to hexa
            }
            case ('x'):
            case ('X'):             /* hexadecimal integer */
            {
                unsigned long long val;
                int imax;
                
                if (modifiers == LL_MOD) // 64 bits
                {
                    val = va_arg( *args, unsigned long long);
                    
                    // if asked to print in base 10, can do only if it fits in 32 bits
                    if (*(format-1) == 'u' && (!(val & 0xFFFFFFFF00000000ULL))) 
                    {
                        unsigned int uintv = (unsigned int) val;
                        
                        for(i = 0; i < 10; i++) 
                        {
                            buf[9 - i] = HexaTab[uintv % 10];
                            if (!(uintv /= 10)) break;
                        }
                        len =  i + 1;
                        pbuf = &buf[9 - i];
                        break;
                    }
                    
                    imax = 16;
                }
                else //32 bits
                {
                    val = va_arg( *args, unsigned int);
                    imax = 8;
                }
                
                ret = sys_call(SYSCALL_TTY_WRITE,
                               (unsigned int)"0x",
                               2,
                               channel,
                               0);
                if (ret != 2) goto return_error;
                
                for(i = 0; i < imax; i++) 
                {
                    buf[(imax-1) - i] = HexaTab[val % 16];
                    if (!(val /= 16))  break;
                }
                len =  i + 1;
                pbuf = &buf[(imax-1) - i];
                break;
            }
            case ('s'):             /* string */
            {
                char* str = va_arg( *args, char* );
                
                if (modifiers != NO_MOD) goto return_error; // Modifiers have no meaning
                
                while (str[len]) 
                {
                    len++;
                }
                pbuf = str;
                break;
            }
            case ('e'):
            case ('f'):
            case ('g'):             /* IEEE754 64 bits */
            {
                union
                {
                    double d;
                    unsigned long long ull;
                } val;
                
                val.d = va_arg( *args, double );
                
                unsigned long long digits = val.ull & 0xFFFFFFFFFFFFFULL;    //get mantissa
                
                unsigned int
                    base = (unsigned int)((val.ull & 0x7FF0000000000000ULL) >> 52), //get exposant
                    intp = (unsigned int)val.d,         //get integer part of the float
                    decp;
                
                int isvalue = 0;
                
                if (base == 0x7FF) //special value
                {
                    if (digits & 0xFFFFFFFFFFFFFULL)
                    {
                        /* Not a Number */
                        buf[0] = 'N';
                        buf[1] = 'a';
                        buf[2] = 'N';
                        len = 3;
                        pbuf = buf;
                    }
                    else
                    {
                        /* inf */
                        buf[0] = (val.ull & 0x8000000000000000ULL) ? '-' : '+';
                        buf[1] = 'i';
                        buf[2] = 'n';
                        buf[3] = 'f';
                        len = 4;
                        pbuf = buf;
                    }
                    break;
                }
                
                if (val.ull & 0x8000000000000000ULL)
                {
                    /* negative */
                    ret = sys_call(SYSCALL_TTY_WRITE,
                                (unsigned int)"-",
                                1,
                                channel,
                                0);
                    if (ret != 1) goto return_error;
                    val.d = val.d * -1;
                }
                else
                {
                    /* positive */
                    ret = sys_call(SYSCALL_TTY_WRITE,
                                (unsigned int)"+",
                                1,
                                channel,
                                0);
                    if (ret != 1) goto return_error;
                }
                
                if (val.d > 0xFFFFFFFF)
                {
                    /* overflow */
                    buf[0] = 'B';
                    buf[1] = 'I';
                    buf[2] = 'G';
                    len = 3;
                    pbuf = buf;
                    break;
                }
                
                val.d -= (double)intp;
                decp = (unsigned int)(val.d * 1000000000);
                
                for(i = 0; i < 10; i++) 
                {
                    if ((!isvalue) && (intp % 10)) isvalue = 1;
                    buf[9 - i] = HexaTab[intp % 10];
                    if (!(intp /= 10)) break;
                }
                pbuf = &buf[9 - i];
                len = i+11;
                buf[10] = '.';
                
                for(i = 0; i < 9; i++)
                {
                    if ((!isvalue) && (decp % 10)) isvalue = 1;
                    buf[19 - i] = HexaTab[decp % 10];
                    decp /= 10;
                }
                
                if (!isvalue)
                {
                    if (val.d != 0)
                    {
                        /* underflow */
                        buf[0] = 'T';
                        buf[1] = 'I';
                        buf[2] = 'N';
                        buf[3] = 'Y';
                        len = 4;
                        pbuf = buf;
                    }
                }

                break;
            }
            case ('l'):
                switch (modifiers)
                {
                    case NO_MOD:
                        modifiers = L_MOD;
                        goto printf_arguments;
                    
                    case L_MOD:
                        modifiers = LL_MOD;
                        goto printf_arguments;
                    
                    default:
                        goto return_error;
                }

            /* Ignored fields : width and precision */
            case ('.'): goto printf_arguments;
                
            default:
                goto return_error;
        }

        ret = sys_call(SYSCALL_TTY_WRITE, 
                       (unsigned int)pbuf,
                       len,
                       channel, 
                       0);
        if (ret != len)  goto return_error;
        
        goto printf_text;
    }

return_error:
    return 1;
} // end __printf()


////////////////////////////////////////
void giet_tty_printf( char* format, ...) 
{
    va_list args;

    va_start( args, format );
    int ret = __printf(format, 0xFFFFFFFF, &args);
    va_end( args );

    if (ret)
    {
        giet_exit("ERROR in giet_tty_printf()");
    }
} // end giet_tty_printf()

/////////////////////////////////
void giet_tty_getc( char * byte ) 
{
    int ret;

    do
    {
        ret = sys_call(SYSCALL_TTY_READ, 
                      (unsigned int)byte,  // buffer address
                      1,                   // number of characters
                      0xFFFFFFFF,          // channel index from task context
                      0);
        if ( ret < 0 ) giet_exit("error in giet_tty_getc()");
    }
    while (ret != 1); 
}

/////////////////////////////////////
void giet_tty_gets( char*        buf, 
                    unsigned int bufsize ) 
{
    int           ret;                           // return value from syscalls
    unsigned char byte;
    unsigned int  index = 0;
    unsigned int  string_cancel = 0x00082008;    // string containing BS/SPACE/BS
 
    while (index < (bufsize - 1)) 
    {
        // get one character
        do 
        { 
            ret = sys_call(SYSCALL_TTY_READ, 
                           (unsigned int)(&byte),
                           1,
                           0xFFFFFFFF,        // channel index from task context
                           0);
            if ( ret < 0 ) giet_exit("error in giet_tty_gets()");
        } 
        while (ret != 1);

        // analyse character
        if (byte == 0x0A)                          // LF  special character
        {
            break; 
        }
        else if ( (byte == 0x7F) ||                // DEL special character
                  (byte == 0x08) )                 // BS  special character
        {
            if ( index > 0 )     
            {
                index--; 

                // cancel character
                ret = sys_call( SYSCALL_TTY_WRITE,
                                (unsigned int)(&string_cancel),
                                3,
                                0XFFFFFFFF,        // channel index from task context
                                0 );
                if ( ret < 0 ) giet_exit("error in giet_tty_gets()");
            }
        }
        else if ( (byte < 0x20) || (byte > 0x7F) )  // non printable characters
        {
        }
        else                                       // take all other characters
        {
            buf[index] = byte;
            index++;

            // echo
            ret = sys_call( SYSCALL_TTY_WRITE,
                            (unsigned int)(&byte),
                            1,
                            0XFFFFFFFF,        // channel index from task context
                            0 );
            if ( ret < 0 ) giet_exit("error in giet_tty_gets()");
     
        }
    }
    buf[index] = 0;

}   // end giet_tty_gets()

///////////////////////////////////////
void giet_tty_getw( unsigned int* val ) 
{
    unsigned char buf[32];
    unsigned int  string_byte   = 0x00000000;    // string containing one single byte 
    unsigned int  string_cancel = 0x00082008;    // string containing BS/SPACE/BS
    unsigned int  save = 0;
    unsigned int  dec = 0;
    unsigned int  done = 0;
    unsigned int  overflow = 0;
    unsigned int  length = 0;
    unsigned int  i;
    int           ret;      // return value from syscalls
 
    // get characters
    while (done == 0) 
    {
        // read one character
        do 
        { 
            ret = sys_call( SYSCALL_TTY_READ,
                            (unsigned int)(&string_byte),
                            1,
                            0xFFFFFFFF,    // channel index from task context
                            0); 
            if ( ret < 0 ) giet_exit("error in giet_tty_getw()");
        } 
        while (ret != 1);

        // analyse character
        if ((string_byte > 0x2F) && (string_byte < 0x3A))  // decimal character 
        {
            buf[length] = (unsigned char)string_byte;
            length++;

            // echo
            ret = sys_call( SYSCALL_TTY_WRITE, 
                            (unsigned int)(&string_byte),
                            1, 
                            0xFFFFFFFF,    // channel index from task context
                            0 );
            if ( ret < 0 ) giet_exit("error in giet_tty_getw()");
        }
        else if (string_byte == 0x0A)                     // LF character 
        {
            done = 1;
        }
        else if ( (string_byte == 0x7F) ||                // DEL character
                  (string_byte == 0x08) )                 // BS  character 
        {
            if ( length > 0 ) 
            {
                length--;    // cancel the character 

                ret = sys_call( SYSCALL_TTY_WRITE, 
                                (unsigned int)(&string_cancel),
                                3, 
                                0xFFFFFFFF,    // channel index from task context
                                0 );
                if ( ret < 0 ) giet_exit("error in giet_tty_getw()");
            }
        }

        // test buffer overflow
        if ( length >= 32 )  
        {
            overflow = 1;
            done     = 1;
        }
    }  // end while characters

    // string to int conversion with overflow detection 
    if ( overflow == 0 )
    {
        for (i = 0; (i < length) && (overflow == 0) ; i++) 
        {
            dec = dec * 10 + (buf[i] - 0x30);
            if (dec < save)  overflow = 1; 
            save = dec;
        }
    } 

    // final evaluation 
    if ( overflow == 0 )
    {
        // return value
        *val = dec;
    }
    else
    {
        // cancel all echo characters
        for (i = 0; i < length ; i++) 
        {
            ret = sys_call( SYSCALL_TTY_WRITE, 
                            (unsigned int)(&string_cancel),
                            3, 
                            0xFFFFFFFF,    // channel index from task context
                            0 );
            if ( ret < 0 ) giet_exit("error in giet_tty_getw()");
        }
        // echo character '0'
        string_byte = 0x30;
        ret = sys_call( SYSCALL_TTY_WRITE, 
                        (unsigned int)(&string_byte),
                        1, 
                        0xFFFFFFFF,    // channel index from task context
                        0 );
        if ( ret < 0 ) giet_exit("error in giet_tty_getw()");

        // return 0 value 
        *val = 0;
    }
}   // end giet_tty_getw()


//////////////////////////////////////////////////////////////////////////////////
/////////////////////  TIMER related system calls //////////////////////////////// 
//////////////////////////////////////////////////////////////////////////////////

///////////////////////
void giet_timer_alloc() 
{
    if ( sys_call( SYSCALL_TIM_ALLOC,
                   0, 0, 0, 0 ) ) giet_exit("error in giet_timer_alloc()");
}

////////////////////////////////////////////
void giet_timer_start( unsigned int period ) 
{
    if ( sys_call( SYSCALL_TIM_START,
                   period,
                   0, 0, 0 ) ) giet_exit("error in giet_timer_start()");
}

//////////////////////
void giet_timer_stop() 
{
    if ( sys_call( SYSCALL_TIM_STOP,
                   0, 0, 0, 0 ) ) giet_exit("error in giet_timer_stop()");
}


//////////////////////////////////////////////////////////////////////////////////
///////////////  Frame buffer device related system calls  ///////////////////////
//////////////////////////////////////////////////////////////////////////////////

/////////////////////////
void giet_fbf_cma_alloc()
{
    if ( sys_call( SYSCALL_FBF_CMA_ALLOC, 
                   0, 0, 0, 0 ) )    giet_exit("error in giet_fbf_cma_alloc()");
}

///////////////////////////////////////////
void giet_fbf_cma_init_buf( void* buf0_vbase, 
                            void* buf1_vbase,
                            void* sts0_vaddr,
                            void* sts1_vaddr )
{
    if ( sys_call( SYSCALL_FBF_CMA_INIT_BUF,
                   (unsigned int)buf0_vbase, 
                   (unsigned int)buf1_vbase,
                   (unsigned int)sts0_vaddr, 
                   (unsigned int)sts1_vaddr ) )   giet_exit("error in giet_fbf_cma_init_buf()");
}

///////////////////////////////////////////
void giet_fbf_cma_start( unsigned int length )
{
    if ( sys_call( SYSCALL_FBF_CMA_START,
                   length, 
                   0, 0, 0 ) )   giet_exit("error in giet_fbf_cma_start()");
}

////////////////////////////////////////////////
void giet_fbf_cma_display( unsigned int buffer )
{
    if ( sys_call( SYSCALL_FBF_CMA_DISPLAY,
                   buffer, 
                   0, 0, 0 ) )   giet_exit("error in giet_fbf_cma_display()");
}

////////////////////////
void giet_fbf_cma_stop()
{
    if ( sys_call( SYSCALL_FBF_CMA_STOP, 
                   0, 0, 0, 0 ) )    giet_exit("error in giet_fbf_cma_stop()");
}

//////////////////////////////////////////////
void giet_fbf_sync_write( unsigned int offset, 
                          void *       buffer, 
                          unsigned int length ) 
{
    if ( sys_call( SYSCALL_FBF_SYNC_WRITE, 
                   offset, 
                   (unsigned int)buffer, 
                   length, 
                   0 ) )  giet_exit("error in giet_fbf_sync_write()");
}

/////////////////////////////////////////////
void giet_fbf_sync_read( unsigned int offset, 
                         void *       buffer, 
                         unsigned int length ) 
{
    if ( sys_call( SYSCALL_FBF_SYNC_READ, 
                   offset, 
                   (unsigned int)buffer, 
                   length, 
                   0 ) )   giet_exit("error in giet_fbf_sync_read()");
}


//////////////////////////////////////////////////////////////////////////////////
/////////////////////// NIC related system calls /////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////
unsigned int giet_nic_rx_alloc( unsigned int xmax,
                                unsigned int ymax )
{
    int channel = sys_call( SYSCALL_NIC_ALLOC,
                            1, 
                            xmax,
                            ymax,
                            0 );
    if ( channel < 0 ) giet_exit("error in giet_nic_rx_alloc()");

    return (unsigned int)channel;
}

////////////////////////////////////////////////////
unsigned int giet_nic_tx_alloc( unsigned int xmax,
                                unsigned int ymax )
{
    int channel = sys_call( SYSCALL_NIC_ALLOC,
                            0,
                            xmax,
                            ymax,
                            0 );
    if ( channel < 0 ) giet_exit("error in giet_nic_tx_alloc()");

    return (unsigned int)channel;
}

//////////////////////////////////////////////
void giet_nic_rx_start( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_START,
                   1,
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_rx_start()");
}

//////////////////////////////////////////////
void giet_nic_tx_start( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_START,
                   0, 
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_tx_start()");
}

///////////////////////////////////////////////////////////
void giet_nic_rx_move( unsigned int channel, void* buffer )
{
    if ( sys_call( SYSCALL_NIC_MOVE,
                   1,
                   channel, 
                   (unsigned int)buffer,
                   0 ) )  giet_exit("error in giet_nic_rx_move()");
}

///////////////////////////////////////////////////////////
void giet_nic_tx_move( unsigned int channel, void* buffer )
{
    if ( sys_call( SYSCALL_NIC_MOVE,
                   0,
                   channel, 
                   (unsigned int)buffer,
                   0 ) )  giet_exit("error in giet_nic_tx_move()");
}

/////////////////////////////////////////////
void giet_nic_rx_stop( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_STOP,
                   1, 
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_rx_stop()");
}

/////////////////////////////////////////////
void giet_nic_tx_stop( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_STOP,
                   0, 
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_tx_stop()");
}

//////////////////////////////////////////////
void giet_nic_rx_stats( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_STATS,
                   1, 
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_rx_stats()");
}

//////////////////////////////////////////////
void giet_nic_tx_stats( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_STATS,
                   0, 
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_tx_stats()");
}

//////////////////////////////////////////////
void giet_nic_rx_clear( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_CLEAR,
                   1, 
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_rx_clear()");
}

//////////////////////////////////////////////
void giet_nic_tx_clear( unsigned int channel )
{
    if ( sys_call( SYSCALL_NIC_CLEAR,
                   0, 
                   channel,
                   0, 0 ) ) giet_exit("error in giet_nic_tx_clear()");
}



///////////////////////////////////////////////////////////////////////////////////
///////////////////// FAT related system calls ////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////
int giet_fat_open( char*        pathname,
                   unsigned int flags ) 
{
    return  sys_call( SYSCALL_FAT_OPEN, 
                      (unsigned int)pathname, 
                      flags,
                      0, 0 );
}

/////////////////////////////////////////
int giet_fat_close( unsigned int fd_id )
{
    return  sys_call( SYSCALL_FAT_CLOSE,
                      fd_id,
                      0, 0, 0 );
}

/////////////////////////////////////////////
int giet_fat_file_info( unsigned int            fd_id,
                        struct fat_file_info_s* info )
{
    return sys_call( SYSCALL_FAT_FINFO,
                     fd_id,
                     (unsigned int)info,
                     0, 0 );
}

///////////////////////////////////////
int giet_fat_read( unsigned int fd_id,     
                   void*        buffer, 
                   unsigned int count )  
{
    return sys_call( SYSCALL_FAT_READ,
                     fd_id,
                     (unsigned int)buffer,
                     count,
                     0 ); 
}

////////////////////////////////////////
int giet_fat_write( unsigned int fd_id,
                    void*        buffer, 
                    unsigned int count )
{
    return sys_call( SYSCALL_FAT_WRITE, 
                     fd_id, 
                     (unsigned int)buffer,
                     count,
                     0 ); 
}

////////////////////////////////////////
int giet_fat_lseek( unsigned int fd_id,
                    unsigned int offset, 
                    unsigned int whence )
{
    return sys_call( SYSCALL_FAT_LSEEK, 
                     fd_id, 
                     offset, 
                     whence,
                     0 ); 
}

////////////////////////////////////////////
int giet_fat_remove( char*         pathname,
                     unsigned int  should_be_dir )
{
    return sys_call( SYSCALL_FAT_REMOVE,
                     (unsigned int)pathname,
                      should_be_dir,
                      0, 0 );
}

/////////////////////////////////////
int giet_fat_rename( char*  old_path,
                     char*  new_path )
{
    return sys_call( SYSCALL_FAT_RENAME,
                     (unsigned int)old_path,
                     (unsigned int)new_path,
                      0, 0 );
}

////////////////////////////////////
int giet_fat_mkdir( char* pathname )
{
    return sys_call( SYSCALL_FAT_MKDIR,
                     (unsigned int)pathname,
                      0, 0, 0 );
}

////////////////////////////////////
int giet_fat_opendir( char* pathname )
{
    return sys_call( SYSCALL_FAT_OPENDIR,
                     (unsigned int)pathname,
                     0, 0, 0 );
}

////////////////////////////////////
int giet_fat_closedir( unsigned int fd_id )
{
    return sys_call( SYSCALL_FAT_CLOSEDIR,
                     (unsigned int)fd_id,
                     0, 0, 0 );
}

////////////////////////////////////
int giet_fat_readdir( unsigned int  fd_id,
                      fat_dirent_t* entry )
{
    return sys_call( SYSCALL_FAT_READDIR,
                     (unsigned int)fd_id,
                     (unsigned int)entry,
                     0, 0 );
}



//////////////////////////////////////////////////////////////////////////////////
///////////////////// Miscellaneous system calls /////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////
void giet_procs_number( unsigned int* x_size, 
                        unsigned int* y_size,
                        unsigned int* nprocs ) 
{
    if ( sys_call( SYSCALL_PROCS_NUMBER, 
                   (unsigned int)x_size, 
                   (unsigned int)y_size, 
                   (unsigned int)nprocs, 
                   0 ) )  giet_exit("ERROR in giet_procs_number()");
}

////////////////////////////////////////////////////
void giet_vobj_get_vbase( char*         vspace_name, 
                          char*         vobj_name, 
                          unsigned int* vbase ) 
{
    if ( sys_call( SYSCALL_VOBJ_GET_VBASE, 
                   (unsigned int) vspace_name,
                   (unsigned int) vobj_name,
                   (unsigned int) vbase,
                   0 ) )  giet_exit("ERROR in giet_vobj_get_vbase()");
}

////////////////////////////////////////////////////
void giet_vobj_get_length( char*         vspace_name, 
                           char*         vobj_name, 
                           unsigned int* length ) 
{
    if ( sys_call( SYSCALL_VOBJ_GET_LENGTH, 
                   (unsigned int) vspace_name,
                   (unsigned int) vobj_name,
                   (unsigned int) length,
                   0 ) )  giet_exit("ERROR in giet_vobj_get_length()");
}

/////////////////////////////////////////
void giet_heap_info( unsigned int* vaddr, 
                     unsigned int* length,
                     unsigned int  x,
                     unsigned int  y ) 
{
    if ( sys_call( SYSCALL_HEAP_INFO, 
                   (unsigned int)vaddr, 
                   (unsigned int)length, 
                   x,
                   y ) )  giet_exit("ERROR in giet_heap_info()");
}

/////////////////////////////////////////
void giet_get_xy( void*         ptr,
                  unsigned int* px,
                  unsigned int* py )
{
    if ( sys_call( SYSCALL_GET_XY,
                   (unsigned int)ptr,
                   (unsigned int)px,
                   (unsigned int)py,
                   0 ) )  giet_exit("ERROR in giet_get_xy()");
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

