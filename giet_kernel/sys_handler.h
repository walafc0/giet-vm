///////////////////////////////////////////////////////////////////////////////
// File     : sys_handler.h
// Date     : 01/04/2012
// Author   : alain greiner 
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////
// The sys_handler.c and sys_handler.h files are part of the GIET-VM kernel.
// It define the syscall_vector[] (at the end of this file), as well as the 
// associated syscall handlers.
///////////////////////////////////////////////////////////////////////////////

#ifndef _SYS_HANDLER_H
#define _SYS_HANDLER_H

#include "giet_config.h"
#include "kernel_locks.h"
#include "stdio.h"

///////////////////////////////////////////////////////////////////////////////
//     Syscall Vector Table (indexed by syscall index)
///////////////////////////////////////////////////////////////////////////////

extern const void * _syscall_vector[64];

///////////////////////////////////////////////////////////////////////////////
// This structure is used by the CMA component to store the status of the
// frame buffer (full or empty). The useful information is contained in the
// "status" integer (1 for full and 0 for empty).
// This structure must be aligned on a cache line (64 bytes) to simplify 
// the software L2/L3 cache coherence when the IO bridge is used. 
///////////////////////////////////////////////////////////////////////////////

typedef struct buffer_status_s
{
    unsigned int status;
    unsigned int padding[15];
} buffer_status_t;

///////////////////////////////////////////////////////////////////////////////
// This structure is used by the CMA component to move a stream 
// of images from two user buffers to the frame buffer in kernel space.
// It contains two chbuf arrays:
// - The SRC chbuf contains two buffers (buf0 & buf1), in user space.
// - The DST cbuf contains one single buffer (fbf), that is the frame buffer.
// Each buffer is described with a 64 bits buffer descriptor:
// - the 26 LSB bits contain bits[6:31] of the buffer physical address
// - the 26 following bits contain bits[6:31] of the physical address where the
//   buffer status is located
// - the 12 MSB bits contain the common address extension of the buffer and its
//   status
// The length field define the buffer size (bytes)
// This structure must be 64 bytes aligned.
///////////////////////////////////////////////////////////////////////////////

typedef struct fbf_chbuf_s
{
    unsigned long long  buf0_desc;     // first user buffer descriptor 
    unsigned long long  buf1_desc;     // second user buffer descriptor
    unsigned long long  fbf_desc;      // frame buffer descriptor 
    unsigned int        length;        // buffer length (bytes)
    unsigned int        padding[9];    // padding for 64 bytes alignment
} fbf_chbuf_t;   

///////////////////////////////////////////////////////////////////////////////
// This structure is used by the CMA component to move a stream of containers 
// between the NIC chbuf containing 2 buffers, and a kernel chbuf 
// containing up to (X_SIZE * Y_SIZE) buffers (one buffer per cluster).
// The same structure is used for both TX or RX transfers.
// The number of distributed containers can be smaller than (X_SIZE * YSIZE).
// The actual number of buffers used in the chbuf is defined by (xmax * ymax).
// Each buffer is described with a 64 bits buffer descriptor:
// - the 26 LSB bits contain bits[6:31] of the buffer physical address
// - the 26 following bits contain bits[6:31] of the physical address where the
//   buffer status is located
// - the 12 MSB bits contain the common address extension of the buffer and its
//   status
// This structure must be 64 bytes aligned.
///////////////////////////////////////////////////////////////////////////////

typedef struct ker_chbuf_s
{
    unsigned long long   buf_desc[X_SIZE*Y_SIZE]; // kernel chbuf descriptor
    unsigned int         xmax;                        // nb clusters in a row
    unsigned int         ymax;                        // nb clusters in a column
} ker_chbuf_t;




///////////////////////////////////////////////////////////////////////////////
//    Coprocessors related syscall handlers
///////////////////////////////////////////////////////////////////////////////

int _sys_coproc_register_set( unsigned int cluster_xy,
                              unsigned int reg_index,
                              unsigned int value );

int _sys_coproc_register_get( unsigned int  cluster_xy,
                              unsigned int  reg_index,
                              unsigned int* buffer );

int _sys_coproc_alloc( unsigned int   coproc_type,
                       unsigned int*  coproc_info );

int _sys_coproc_release( unsigned int coproc_reg_index );

int _sys_coproc_channel_init( unsigned int            channel,
                              giet_coproc_channel_t*  desc );

int _sys_coproc_run( unsigned int coproc_reg_index );

int _sys_coproc_completed();

///////////////////////////////////////////////////////////////////////////////
//    TTY related syscall handlers
///////////////////////////////////////////////////////////////////////////////

int _sys_tty_alloc( unsigned int shared );

int _sys_tty_release();

int _sys_tty_write( const char*  buffer,
                    unsigned int length,
                    unsigned int channel );

int _sys_tty_read(  char*        buffer,
                    unsigned int length,
                    unsigned int channel );

//////////////////////////////////////////////////////////////////////////////
//    TIM related syscall handlers
//////////////////////////////////////////////////////////////////////////////

int _sys_tim_alloc();

int _sys_tim_release();

int _sys_tim_start( unsigned int period );

int _sys_tim_stop();

//////////////////////////////////////////////////////////////////////////////
//    NIC related syscall handlers
//////////////////////////////////////////////////////////////////////////////

int _sys_nic_alloc( unsigned int is_rx,
                    unsigned int xmax,
                    unsigned int ymax );

int _sys_nic_release( unsigned int is_rx );

int _sys_nic_start( unsigned int is_rx,
                    unsigned int channel );

int _sys_nic_move( unsigned int is_rx,
                   unsigned int channel,
                   void*        buffer );

int _sys_nic_stop( unsigned int is_rx,
                   unsigned int channel );

int _sys_nic_clear( unsigned int is_rx,
                    unsigned int channel );

int _sys_nic_stats( unsigned int is_rx,
                    unsigned int channel );

//////////////////////////////////////////////////////////////////////////////
//    FBF related syscall handlers
//////////////////////////////////////////////////////////////////////////////

int _sys_fbf_sync_write( unsigned int offset,
                         void*        buffer,
                         unsigned int length );

int _sys_fbf_sync_read(  unsigned int offset,
                         void*        buffer,
                         unsigned int length );

int _sys_fbf_cma_alloc();

int _sys_fbf_cma_release();

int _sys_fbf_cma_init_buf(void*        buf0_vbase, 
                          void*        buf1_vbase, 
                          void*        sts0_vaddr,
                          void*        sts1_vaddr );

int _sys_fbf_cma_start( unsigned int length );

int _sys_fbf_cma_display( unsigned int buffer_index );

int _sys_fbf_cma_stop();

//////////////////////////////////////////////////////////////////////////////
//    Miscelaneous syscall handlers
//////////////////////////////////////////////////////////////////////////////

int _sys_ukn();

int _sys_proc_xyp( unsigned int* x,
                   unsigned int* y,
                   unsigned int* p );

int _sys_task_exit( char* string );

int _sys_kill_application( char* name );

int _sys_exec_application( char* name );

int _sys_context_switch();

int _sys_local_task_id();

int _sys_global_task_id();

int _sys_thread_id();

int _sys_procs_number( unsigned int* x_size,
                       unsigned int* y_size, 
                       unsigned int* nprocs );

int _sys_vseg_get_vbase( char*         vspace_name,
                         char*         vseg_name,
                         unsigned int* vbase );

int _sys_vseg_get_length( char*         vspace_name, 
                          char*         vseg_name,
                          unsigned int* length );

int _sys_xy_from_ptr( void*          ptr,
                      unsigned int*  x,
                      unsigned int*  y );

int _sys_heap_info( unsigned int* vaddr, 
                    unsigned int* length,
                    unsigned int  x,
                    unsigned int  y ); 

int _sys_tasks_status();

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

