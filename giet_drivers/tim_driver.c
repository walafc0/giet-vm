/////////////////////////////////////////////////////////////////////////////
// File     : tim_driver.c
// Date     : 23/05/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
/////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <tim_driver.h>
#include <utils.h>
#include <tty0.h>

#if !defined(SEG_TIM_BASE)
# error: You must define SEG_TIM_BASE in the hard_config.h file
#endif

#if !defined(PERI_CLUSTER_INCREMENT) 
# error: You must define PERI_CLUSTER_INCREMENT in the hard_config.h file
#endif

#if !defined(X_SIZE) 
# error: You must define X_SIZE in the hard_config.h file
#endif

#if !defined(Y_SIZE) 
# error: You must define X_SIZE in the hard_config.h file
#endif

#if !defined(X_WIDTH) 
# error: You must define X_WIDTH in the hard_config.h file
#endif

#if !defined(Y_WIDTH) 
# error: You must define X_WIDTH in the hard_config.h file
#endif

#if !defined(NB_PROCS_MAX) 
# error: You must define NB_PROCS_MAX in the hard_config.h file
#endif

#if !defined(NB_TIM_CHANNELS)
#define NB_TIM_CHANNELS 0
#endif

#if ( (NB_TIM_CHANNELS + NB_PROC_MAX) > 32 )
# error: NB_TIM_CHANNELS + NB_PROCS_MAX cannot be larger than 32
#endif

/////////////////////////////////////////////////////////////////////////////
//                      global variables
/////////////////////////////////////////////////////////////////////////////

#define in_unckdata __attribute__((section (".unckdata")))

#if (NB_TIM_CHANNELS > 0)
in_unckdata volatile unsigned char _user_timer_event[NB_TIM_CHANNELS]
                        = { [0 ... ((1<<NB_TIM_CHANNELS) - 1)] = 0 };
#endif

/////////////////////////////////////////////////////////////////////////////
//                      access functions
/////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////
unsigned int _timer_get_register( unsigned int channel,
                                  unsigned int index )
{
    unsigned int* vaddr = (unsigned int*)SEG_TIM_BASE + channel*TIMER_SPAN + index;
    return _io_extended_read( vaddr );
}

//////////////////////////////////////////////////////
void _timer_set_register( unsigned int channel,
                          unsigned int index,
                          unsigned int value )
{
    unsigned int* vaddr = (unsigned int*)SEG_TIM_BASE + channel*TIMER_SPAN + index;
    _io_extended_write( vaddr, value );
}

///////////////////////////////////////
int _timer_start( unsigned int channel, 
                  unsigned int period ) 
{
#if NB_TIM_CHANNELS

    if ( channel >= NB_TIM_CHANNELS )
    {
        _puts("[GIET ERROR] in _timer_start()\n");
        return -1;
    }

    _timer_set_register( channel, TIMER_PERIOD, period );
    _timer_set_register( channel, TIMER_MODE  , 0x3 );
    
    return 0;

#else

    _puts("[GIET ERROR] _timer_start() should not be called when NB_TIM_CHANNELS is 0\n");
    return -1;

#endif
}

///////////////////////////////////////
int _timer_stop( unsigned int channel ) 
{
#if NB_TIM_CHANNELS

    if ( channel >= NB_TIM_CHANNELS )
    {
        _puts("[GIET ERROR] in _timer_stop()\n");
        return -1;
    }

    _timer_set_register( channel, TIMER_MODE  , 0 );

    return 0;

#else

    _puts("[GIET ERROR] _timer_stop() should not be called when NB_TIM_CHANNELS is 0\n");
    return -1;

#endif
}

////////////////////////////////////////////
int _timer_reset_cpt( unsigned int channel ) 
{
#if NB_TIM_CHANNELS

    if ( channel >= NB_TIM_CHANNELS )
    {
        _puts("[GIET ERROR in _timer_reset_cpt()\n");
        return -1;
    }

    unsigned int period = _timer_get_register( channel, TIMER_PERIOD );
    _timer_set_register( channel, TIMER_PERIOD, period );

    return 0;

#else

    _puts("[GIET ERROR] _timer_reset_cpt should not be called when NB_TIM_CHANNELS is 0\n");
    return -1;

#endif
}

///////////////////////////////////////
void _timer_isr( unsigned int irq_type,   // HWI / PTI
                 unsigned int irq_id,     // index returned by XCU
                 unsigned int channel )   // user timer index
{
#if NB_TIM_CHANNELS

    // acknowledge IRQ
    _timer_set_register( channel, TIMER_RESETIRQ, 0 ); 

    // register the event
    _user_timer_event[channel] = 1;

    // display a message on TTY 0 
    _puts("\n[GIET WARNING] User Timer IRQ at cycle %d for channel = %d\n",
            _get_proctime(), channel );

#else

    _puts("[GIET ERROR] _timer_isr() should not be called when NB_TIM_CHANNELS == 0\n");
    _exit();

#endif
}



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

