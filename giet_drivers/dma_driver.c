///////////////////////////////////////////////////////////////////////////////////
// File     : dma_driver.c
// Date     : 23/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <dma_driver.h>
#include <tty0.h>
#include <vmem.h>
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

#if !defined(NB_DMA_CHANNELS) 
# error: You must define NB_DMA_CHANNELS in the hard_config.h file
#endif

#if !defined(SEG_DMA_BASE) 
# error: You must define SEG_DMA_BASE in the hard_config.h file
#endif

#if !defined(PERI_CLUSTER_INCREMENT) 
# error: You must define PERI_CLUSTER_INCREMENT in the hard_config.h file
#endif

extern volatile unsigned int _ptabs_vaddr[];

///////////////////////////////////////////////////////////////////////////////
// This low level function returns the value contained in register "index"
// in the DMA component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////

#if NB_DMA_CHANNELS > 0
static
unsigned int _dma_get_register( unsigned int cluster_xy, // cluster index
                                unsigned int channel_id, // channel index
                                unsigned int index )     // register index
{
    unsigned int vaddr =
        SEG_DMA_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        (channel_id * DMA_SPAN) +
        (index << 2);

    return ioread32( (void*)vaddr );
}
#endif

///////////////////////////////////////////////////////////////////////////////
// This low level function sets a new value in register "index"
// in the DMA component contained in cluster "cluster_xy"
///////////////////////////////////////////////////////////////////////////////

#if NB_DMA_CHANNELS > 0
static
void _dma_set_register( unsigned int cluster_xy,       // cluster index
                        unsigned int channel_id,       // channel index
                        unsigned int index,            // register index
                        unsigned int value )           // value to be written
{
    unsigned int vaddr =
        SEG_DMA_BASE + 
        (cluster_xy * PERI_CLUSTER_INCREMENT) +
        (channel_id * DMA_SPAN) +
        (index << 2);

    iowrite32( (void*)vaddr, value );
}
#endif

////////////////////////////////////////////////
void _dma_disable_irq( unsigned int cluster_xy,
                       unsigned int channel_id )
{
#if NB_DMA_CHANNELS > 0

    // check DMA channel parameters 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if ( (x >= X_SIZE) || (y >= Y_SIZE) || (channel_id >= NB_DMA_CHANNELS) )
    {
        _puts("\n[DMA ERROR] in _dma_disable_irq() : illegal DMA channel ");
        _exit();
    }

    // disable interrupt for selected channel
    _dma_set_register(cluster_xy, channel_id, DMA_IRQ_DISABLE, 1);

#endif
}

/////////////////////////////////////////////////
void _dma_reset_channel( unsigned int cluster_xy, 
                         unsigned int channel_id ) 
{
#if NB_DMA_CHANNELS > 0

    // check DMA channel parameters 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if ( (x >= X_SIZE) || (y >= Y_SIZE) || (channel_id >= NB_DMA_CHANNELS) )
    {
        _puts("\n[DMA ERROR] in _dma_reset_channel() : illegal DMA channel ");
        _exit();
    }

    // reset selected channel
    _dma_set_register(cluster_xy, channel_id, DMA_RESET, 0);

#endif
}

///////////////////////////////////////////////////////
void _dma_get_status( unsigned int  cluster_xy, 
                      unsigned int  channel_id,
                      unsigned int* status ) 
{
#if NB_DMA_CHANNELS > 0

    // check DMA channel parameters 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if ( (x >= X_SIZE) || (y >= Y_SIZE) || (channel_id >= NB_DMA_CHANNELS) )
    {
        _puts("\n[DMA ERROR] in _dma_get_status() : illegal DMA channel ");
        _exit();
    }

    // returns selected channel status
    *status = _dma_get_register(cluster_xy, channel_id, DMA_LEN);

#endif
}

////////////////////////////////////////////////////////
void _dma_start_transfer( unsigned int       cluster_xy,  // DMA cluster
                          unsigned int       channel_id,  // DMA channel
                          unsigned long long dst_paddr,   // physical address
                          unsigned long long src_paddr,   // physical address
                          unsigned int       size )       // bytes
{
#if NB_DMA_CHANNELS > 0

    // check DMA channel parameters 
    unsigned int x = cluster_xy >> Y_WIDTH;
    unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
    if ( (x >= X_SIZE) || (y >= Y_SIZE) || (channel_id >= NB_DMA_CHANNELS) )
    {
        _puts("\n[DMA ERROR] in _dma_start_transfer() : illegal DMA channel ");
        _exit();
    }

    // selected channel configuration and lauching
    _dma_set_register(cluster_xy, channel_id, DMA_SRC,
            (unsigned int)(src_paddr));
    _dma_set_register(cluster_xy, channel_id, DMA_SRC_EXT,
            (unsigned int)(src_paddr>>32));
    _dma_set_register(cluster_xy, channel_id, DMA_DST,
            (unsigned int)(dst_paddr));
    _dma_set_register(cluster_xy, channel_id, DMA_DST_EXT,
            (unsigned int)(dst_paddr>>32));
    _dma_set_register(cluster_xy, channel_id, DMA_LEN,
            (unsigned int)size);
#endif
}

///////////////////////////////////////////////////////
void _dma_physical_copy( unsigned int       cluster_xy,  // DMA cluster
                         unsigned int       channel_id,  // DMA channel
                         unsigned long long dst_paddr,   // dest physical address
                         unsigned long long src_paddr,   // src physical address
                         unsigned int       size )       // bytes
{
#if NB_DMA_CHANNELS > 0

    // check buffers alignment constraints
    if ( (dst_paddr & 0x3)   || (src_paddr & 0x3) || (size & 0x3) )
    {
        _puts("\n[DMA ERROR] in _dma_physical_copy() : buffer unaligned\n");
        _exit();
    }

#if GIET_DEBUG_DMA_DRIVER
unsigned int x = cluster_xy >> Y_WIDTH;
unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
_puts("\n[DMA DEBUG] enter _dma_physical_copy() for channel[");
_putd( x );
_puts(",");
_putd( y );
_puts(",");
_putd( channel_id );
_puts("] at cycle ");
_putd( _get_proctime() );
_puts("\n - src_paddr   = ");
_putl( src_paddr );
_puts("\n - dst_paddr   = ");
_putl( dst_paddr );
_puts("\n - bytes       = ");
_putx( size );
_puts("\n");
#endif

    // dma channel configuration 
    _dma_disable_irq( cluster_xy, channel_id );

    // dma transfer lauching
    _dma_start_transfer( cluster_xy, channel_id, dst_paddr, src_paddr, size ); 

    // scan dma channel status 
    unsigned int status;
    do
    {
        _dma_get_status( cluster_xy, channel_id , &status );

#if GIET_DEBUG_DMA_DRIVER
_puts("\n[DMA DEBUG] _dma_physical_copy() : ... waiting on DMA_STATUS\n");
#endif

    }
    while( (status != DMA_SUCCESS) && 
           (status != DMA_READ_ERROR) &&
           (status != DMA_WRITE_ERROR) );
    
    // analyse status
    if( status != DMA_SUCCESS )
    {
        _puts("\n[DMA ERROR] in _dma_physical_copy() : ERROR_STATUS");
        _exit();
    }

    // reset dma channel
    _dma_reset_channel( cluster_xy , channel_id );

#if GIET_DEBUG_DMA_DRIVER
_puts("\n[DMA DEBUG] exit _dma_physical_copy() at cycle ");
_putd( _get_proctime() );
_puts("\n");
#endif

#else // NB_DMA_CHANNELS == 0

    _puts("\n[DMA ERROR] in _dma_physical_copy() : NB_DMA_CHANNELS == 0\n");
    _exit();

#endif
}


////////////////////////////////////////
void  _dma_copy( unsigned int cluster_xy,    // DMA cluster
                 unsigned int channel_id,    // DMA channel
                 unsigned int dst_vaddr,     // dst_vaddr buffer vbase
                 unsigned int src_vaddr,     // src_vaddr buffer vbase
                 unsigned int size )         // bytes
{
#if NB_DMA_CHANNELS > 0

    // check buffers alignment constraints
    if ( (dst_vaddr & 0x3)   || (src_vaddr & 0x3) || (size & 0x3) )
    {
        _puts("\n[DMA ERROR] in _dma_copy() : buffer unaligned\n");
        _exit();
    }

    unsigned long long src_paddr;
    unsigned long long dst_paddr;
    unsigned int flags;

#if GIET_DEBUG_DMA_DRIVER
unsigned int x = cluster_xy >> Y_WIDTH;
unsigned int y = cluster_xy & ((1<<Y_WIDTH)-1);
_puts("\n[DMA DEBUG] enter _dma_copy() for channel[");
_putd( x );
_puts(",");
_putd( y );
_puts(",");
_putd( channel_id );
_puts("] at cycle ");
_putd( _get_proctime() );
_puts("\n - src_vaddr   = ");
_putx( src_vaddr );
_puts("\n - dst_vaddr   = ");
_putx( dst_vaddr );
_puts("\n - bytes       = ");
_putd( size );
_puts("\n");
#endif

    // get src_paddr buffer physical addresse
    src_paddr = _v2p_translate( src_vaddr , &flags );

    // get dst_paddr buffer physical addresse
    dst_paddr = _v2p_translate( dst_vaddr , &flags );

#if GIET_DEBUG_DMA_DRIVER
_puts("\n - src_paddr   = ");
_putl( src_paddr );
_puts("\n - dst_paddr   = ");
_putl( dst_paddr );
_puts("\n");
#endif

    // dma channel configuration & lauching
    _dma_start_transfer(  cluster_xy, channel_id, dst_paddr, src_paddr, size ); 

    // scan dma channel status 
    unsigned int status;
    do
    {
        _dma_get_status( cluster_xy, channel_id , &status );

#if GIET_DEBUG_DMA_DRIVER
_puts("\n[DMA DEBUG] _dma_physical_copy() : ... waiting on DMA_STATUS\n");
#endif

    }
    while( (status != DMA_SUCCESS) && 
           (status != DMA_READ_ERROR) &&
           (status != DMA_WRITE_ERROR) );
    
    // analyse status
    if( status != DMA_SUCCESS )
    {
        _puts("\n[DMA ERROR] in _dma_copy() : bad DMA_STATUS\n");
        _exit();
    }
    // reset dma channel
    _dma_reset_channel( cluster_xy, channel_id );

#if GIET_DEBUG_DMA_DRIVER
_puts("\n[DMA DEBUG] exit _dma_copy() at cycle ");
_putd( _get_proctime() );
_puts("\n");
#endif

#else // NB_DMA_CHANNELS == 0

    _puts("\n[DMA ERROR] in _dma_copy() : NB_DMA_CHANNELS == 0\n");
    _exit();

#endif
} // end _dma_copy


/////////////////////////////////////
void _dma_isr( unsigned int irq_type,
               unsigned int irq_id,
               unsigned int channel )
{
    _puts("\n[DMA ERROR] _dma_isr() not implemented\n");
    _exit();
}




// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

