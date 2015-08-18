///////////////////////////////////////////////////////////////////////////////////
// File     : mmc_driver.c
// Date     : 23/05/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <hard_config.h>
#include <mmc_driver.h>
#include <tty0.h>
#include <kernel_locks.h>
#include <utils.h>

#include <io.h>

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

#if !defined(SEG_MMC_BASE) 
# error: You must define SEG_MMC_BASE in the hard_config.h file
#endif

#if !defined(PERI_CLUSTER_INCREMENT) 
# error: You must define PERI_CLUSTER_INCREMENT in the hard_config.h file
#endif

///////////////////////////////////////////////////////////////////////////////
//     Global variables
///////////////////////////////////////////////////////////////////////////////
// Two kinds of locks protecting the MMC components (one per cluster):
// - the _mmc_lock array contains spin_locks allocated in cluster[0,0].
//   They must be used by the boot code because the kernel heap is not set.
// - the _mmc_distributed_locks array contains pointers on distributed
//   spin_loks allocated in the distributed heap in each cluster. 
//   Each cluster contains the lock protecting its own mmc_component.
//   They can be used by the kernel code. 
// The global variable mmc_boot_mode define the type of lock which is used,
// and must be defined in both kernel_init.c and boot.c files.
///////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
unsigned int  _mmc_boot_mode;

__attribute__((section(".kdata")))
spin_lock_t           _mmc_lock[X_SIZE][Y_SIZE]  __attribute__((aligned(64)));

__attribute__((section(".kdata")))
spin_lock_t*          _mmc_distributed_lock[X_SIZE][Y_SIZE];

//////////////////////////////////////////////////////////////////////////////
// This function initializes the distributed locks stored in the kernel heaps
//////////////////////////////////////////////////////////////////////////////

void _mmc_init_locks()
{
    unsigned int x;    // cluster X coordinate
    unsigned int y;    // cluster Y coordinate
    
    for ( x = 0 ; x < X_SIZE ; x++ )
    {
        for ( y = 0 ; y < Y_SIZE ; y++ )
        {
            if ( _mmc_boot_mode )
            {
                _spin_lock_init( &_mmc_lock[x][y] );
            }
            else
            {
                _mmc_distributed_lock[x][y] = _remote_malloc( sizeof(spin_lock_t), x, y );
                _spin_lock_init( _mmc_distributed_lock[x][y] );
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// This low level function returns the value contained in register 
// defined by the ("func" / "index") arguments,
// in the MMC component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////
static
unsigned int _mmc_get_register( unsigned int cluster_xy, // cluster index
                                unsigned int func,       // function index
                                unsigned int index )     // register index
{
    unsigned int vaddr =
        SEG_MMC_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        (MMC_REG(func, index) << 2);

    return ioread32( (void*)vaddr );
}

///////////////////////////////////////////////////////////////////////////////
// This low level function sets a new value in register
// defined by the ("func" / "index") arguments,
// in the MMC component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////
static
void _mmc_set_register( unsigned int cluster_xy,       // cluster index
                        unsigned int func,             // func index
                        unsigned int index,            // register index
                        unsigned int value )           // value to be written
{
    unsigned int vaddr =
        SEG_MMC_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        (MMC_REG(func, index) << 2);
        
    iowrite32( (void*)vaddr, value );
}

/////////////////////////////////////////
void _mmc_inval( paddr_t      buf_paddr,
                 unsigned int buf_length )
{
    // compute cluster coordinates
    unsigned int cluster_xy = (unsigned int)(buf_paddr>>(40-X_WIDTH-Y_WIDTH));
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);

    // parameters checking 
    if ( (x >= X_SIZE) || (y >= Y_SIZE) )
    {
        _puts("\n[GIET ERROR] in _mmc_inval() : illegal cluster coordinates\n");
        _exit();
    }

    if ( buf_paddr & 0x3F )
    {
        _puts("\n[GIET ERROR] in _mmc_inval() : paddr not 64 bytes aligned\n");
        _exit();
    }

    // get the lock protecting exclusive access to MEMC
    if ( _mmc_boot_mode ) _spin_lock_acquire( &_mmc_lock[x][y] );
    else                  _spin_lock_acquire( _mmc_distributed_lock[x][y] );

    // write inval arguments
    _mmc_set_register(cluster_xy, 0, MEMC_ADDR_LO   , (unsigned int)buf_paddr );
    _mmc_set_register(cluster_xy, 0, MEMC_ADDR_HI   , (unsigned int)(buf_paddr>>32) );
    _mmc_set_register(cluster_xy, 0, MEMC_BUF_LENGTH, buf_length );
    _mmc_set_register(cluster_xy, 0, MEMC_CMD_TYPE  , MEMC_CMD_INVAL );

    // release the lock 
    if ( _mmc_boot_mode ) _spin_lock_release( &_mmc_lock[x][y] );
    else                  _spin_lock_release( _mmc_distributed_lock[x][y] );
}

///////////////////////////////////////
void _mmc_sync( paddr_t      buf_paddr,
                unsigned int buf_length )
{
    // compute cluster coordinates
    unsigned int cluster_xy = (unsigned int)(buf_paddr>>(40-X_WIDTH-Y_WIDTH));
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);

    // parameters checking 
    if ( (x >= X_SIZE) || (y >= Y_SIZE) )
    {
        _puts( "\n[GIET ERROR] in _mmc_sync() : illegal cluster coordinates");
        _exit();
    }

    if ( buf_paddr & 0x3F )
    {
        _puts("\n[GIET ERROR] in _mmc_sync() : paddr not 64 bytes aligned\n");
        _exit();
    }

    // get the lock protecting exclusive access to MEMC
    if ( _mmc_boot_mode ) _spin_lock_acquire( &_mmc_lock[x][y] );
    else                  _spin_lock_acquire( _mmc_distributed_lock[x][y] );

    // write inval arguments
    _mmc_set_register(cluster_xy, 0, MEMC_ADDR_LO   , (unsigned int)buf_paddr);
    _mmc_set_register(cluster_xy, 0, MEMC_ADDR_HI   , (unsigned int)(buf_paddr>>32));
    _mmc_set_register(cluster_xy, 0, MEMC_BUF_LENGTH, buf_length);
    _mmc_set_register(cluster_xy, 0, MEMC_CMD_TYPE  , MEMC_CMD_SYNC);

    // release the lock 
    if ( _mmc_boot_mode ) _spin_lock_release( &_mmc_lock[x][y] );
    else                  _spin_lock_release( _mmc_distributed_lock[x][y] );
}

/////////////////////////////////////////////
unsigned int _mmc_instrument( unsigned int x, 
                              unsigned int y,
                              unsigned int reg )
{
    // parameters checking 
    if ( (x >= X_SIZE) || (y >= Y_SIZE) )
    {
        _puts( "\n[GIET ERROR] in _mmc_instrument() : illegal cluster coordinates");
        _exit();
    }

    unsigned int cluster_xy = (x << Y_WIDTH) + y;
    return( _mmc_get_register(cluster_xy , 1 , reg) );
}

///////////////////////////////////////////////////////
void _mmc_isr( unsigned int irq_type,  // should be HWI 
               unsigned int irq_id,    // index returned by ICU
               unsigned int channel )  // unused
{
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int p          = gpid & ((1<<P_WIDTH)-1);

    _puts("[GIET ERROR] MMC IRQ received by processor[");
    _putd( x );
    _puts(",");
    _putd( y );
    _puts(",");
    _putd( p );
    _puts("] but _mmc_isr() not implemented\n");
}



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

