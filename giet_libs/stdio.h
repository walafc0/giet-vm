/////////////////////////////////////////////////////////////////////////////
// File     : stdio.h         
// Date     : 01/04/2010
// Author   : alain greiner & Joel Porquet
// Copyright (c) UPMC-LIP6
/////////////////////////////////////////////////////////////////////////////
// The stdio.c and stdio.h files are part of the GIET_VM nano-kernel.
// This library contains all user-level functions that contain a system call
// to access protected or shared ressources.
/////////////////////////////////////////////////////////////////////////////

#ifndef _STDIO_H
#define _STDIO_H

#include "giet_fat32/fat32_shared.h"

// These define must be synchronised with 
// the _syscall_vector defined in file sys_handler.c

#define SYSCALL_PROC_XYP             0x00
#define SYSCALL_PROC_TIME            0x01
#define SYSCALL_TTY_WRITE            0x02
#define SYSCALL_TTY_READ             0x03
#define SYSCALL_TTY_ALLOC            0x04
#define SYSCALL_TASKS_STATUS         0x05
//                                   0x06
#define SYSCALL_HEAP_INFO            0x07
#define SYSCALL_LOCAL_TASK_ID        0x08
#define SYSCALL_GLOBAL_TASK_ID       0x09
#define SYSCALL_FBF_CMA_ALLOC        0x0A
#define SYSCALL_FBF_CMA_INIT_BUF     0x0B
#define SYSCALL_FBF_CMA_START        0x0C
#define SYSCALL_FBF_CMA_DISPLAY      0x0D
#define SYSCALL_FBF_CMA_STOP         0x0E
#define SYSCALL_EXIT                 0x0F

#define SYSCALL_PROCS_NUMBER         0x10
#define SYSCALL_FBF_SYNC_WRITE       0x11
#define SYSCALL_FBF_SYNC_READ        0x12
#define SYSCALL_THREAD_ID            0x13
#define SYSCALL_TIM_ALLOC            0x14
#define SYSCALL_TIM_START            0x15
#define SYSCALL_TIM_STOP             0x16
#define SYSCALL_KILL_APP             0x17
#define SYSCALL_EXEC_APP             0x18
#define SYSCALL_CTX_SWITCH           0x19
#define SYSCALL_VOBJ_GET_VBASE       0x1A
#define SYSCALL_VOBJ_GET_LENGTH      0x1B
#define SYSCALL_GET_XY               0x1C
//                                   0x1D
//                                   0x1E
//                                   0x1F

#define SYSCALL_FAT_OPEN             0x20
#define SYSCALL_FAT_READ             0x21
#define SYSCALL_FAT_WRITE            0x22
#define SYSCALL_FAT_LSEEK            0x23
#define SYSCALL_FAT_FINFO            0x24
#define SYSCALL_FAT_CLOSE            0x25
#define SYSCALL_FAT_REMOVE           0x26
#define SYSCALL_FAT_RENAME           0x27
#define SYSCALL_FAT_MKDIR            0x28
#define SYSCALL_FAT_OPENDIR          0x29
#define SYSCALL_FAT_CLOSEDIR         0x2A
#define SYSCALL_FAT_READDIR          0x2B
//                                   0x2C
//                                   0x2D
//                                   0x2E
//                                   0x2F

#define SYSCALL_NIC_ALLOC            0x30
#define SYSCALL_NIC_START            0x31
#define SYSCALL_NIC_MOVE             0x32
#define SYSCALL_NIC_STOP             0x33
#define SYSCALL_NIC_STATS            0x34
#define SYSCALL_NIC_CLEAR            0x35
//                                   0x36
//                                   0x37
//                                   0x38
//                                   0x39
//                                   0x3A
#define SYSCALL_COPROC_COMPLETED     0x3B
#define SYSCALL_COPROC_ALLOC         0x3C
#define SYSCALL_COPROC_CHANNEL_INIT  0x3D
#define SYSCALL_COPROC_RUN           0x3E
#define SYSCALL_COPROC_RELEASE       0x3F

////////////////////////////////////////////////////////////////////////////
// NULL pointer definition
////////////////////////////////////////////////////////////////////////////

#define NULL (void *)0

////////////////////////////////////////////////////////////////////////////
// This generic C function is used to implement all system calls.
// It writes the system call arguments in the proper registers,
// and tells GCC what has been modified by system call execution.
// Returns -1 to signal an error.
////////////////////////////////////////////////////////////////////////////
static inline int sys_call( int call_no,
                            int arg_0, 
                            int arg_1, 
                            int arg_2, 
                            int arg_3 ) 
{
    register int reg_no_and_output asm("v0") = call_no;
    register int reg_a0 asm("a0") = arg_0;
    register int reg_a1 asm("a1") = arg_1;
    register int reg_a2 asm("a2") = arg_2;
    register int reg_a3 asm("a3") = arg_3;

    asm volatile(
            "syscall"
            : "+r" (reg_no_and_output), /* input/output argument */
              "+r" (reg_a0),             
              "+r" (reg_a1),
              "+r" (reg_a2),
              "+r" (reg_a3),
              "+r" (reg_no_and_output)
            : /* input arguments */
            : "memory",
            /* These persistant registers will be saved on the stack by the
             * compiler only if they contain relevant data. */
            "at",
            "v1",
            "ra",
            "t0",
            "t1",
            "t2",
            "t3",
            "t4",
            "t5",
            "t6",
            "t7",
            "t8",
            "t9"
               );
    return (volatile int)reg_no_and_output;
}

//////////////////////////////////////////////////////////////////////////
//               MIPS32 related system calls 
//////////////////////////////////////////////////////////////////////////

extern void giet_proc_xyp( unsigned int* cluster_x,
                           unsigned int* cluster_y,
                           unsigned int* lpid );

extern unsigned int giet_proctime();

extern unsigned int giet_rand();

//////////////////////////////////////////////////////////////////////////
//              Task related system calls
//////////////////////////////////////////////////////////////////////////

extern unsigned int giet_proc_task_id();

extern unsigned int giet_global_task_id(); 

extern unsigned int giet_thread_id(); 

extern void giet_exit( char* string );

extern void giet_assert( unsigned int condition, 
                         char*        string );

extern void giet_context_switch();

extern void giet_tasks_status();

//////////////////////////////////////////////////////////////////////////
//               Application related system calls
//////////////////////////////////////////////////////////////////////////

extern int giet_kill_application( char* name );

extern int giet_exec_application( char* name );

//////////////////////////////////////////////////////////////////////////
//             Coprocessors related system calls 
//////////////////////////////////////////////////////////////////////////

// this structure is used by the giet_coproc_channel_init() 
// system call to specify the communication channel parameters. 
typedef struct giet_coproc_channel
{
    unsigned int  channel_mode;    // MWMR / DMA_IRQ / DMA_NO_IRQ
    unsigned int  buffer_size;     // memory buffer size
    unsigned int  buffer_vaddr;    // memory buffer virtual address
    unsigned int  mwmr_vaddr;      // MWMR descriptor virtual address
    unsigned int  lock_vaddr;      // lock for MWMR virtual address
} giet_coproc_channel_t;

extern void giet_coproc_alloc( unsigned int   coproc_type,
                               unsigned int*  coproc_info );

extern void giet_coproc_release( unsigned int coproc_reg_index );

extern void giet_coproc_channel_init( unsigned int            channel,
                                      giet_coproc_channel_t*  desc );

extern void giet_coproc_run( unsigned int coproc_reg_index );

extern void giet_coproc_completed();

//////////////////////////////////////////////////////////////////////////
//             TTY device related system calls 
//////////////////////////////////////////////////////////////////////////

extern void giet_tty_alloc( unsigned int shared );

extern void giet_tty_printf( char* format, ... );

extern void giet_tty_getc( char* byte );

extern void giet_tty_gets( char* buf, unsigned int bufsize );

extern void giet_tty_getw( unsigned int* val );

//////////////////////////////////////////////////////////////////////////
//                TIMER device related system calls 
//////////////////////////////////////////////////////////////////////////

extern void giet_timer_alloc();

extern void giet_timer_start( unsigned int period );

extern void giet_timer_stop();
 
//////////////////////////////////////////////////////////////////////////
//                Frame buffer device related system calls 
//////////////////////////////////////////////////////////////////////////

extern void giet_fbf_cma_alloc();

extern void giet_fbf_cma_init_buf( void* buf0_vbase, 
                                   void* buf1_vbase,
                                   void* sts0_vaddr,
                                   void* sts1_vaddr );

extern void giet_fbf_cma_start( unsigned int length );

extern void giet_fbf_cma_display( unsigned int buffer );

extern void giet_fbf_cma_stop();

extern void giet_fbf_sync_read( unsigned int offset, 
                                void*        buffer, 
                                unsigned int length );

extern void giet_fbf_sync_write( unsigned int offset, 
                                 void*        buffer, 
                                 unsigned int length );

//////////////////////////////////////////////////////////////////////////
//                  NIC related system calls 
//////////////////////////////////////////////////////////////////////////

extern unsigned int giet_nic_rx_alloc( unsigned int xmax, unsigned int ymax );

extern unsigned int giet_nic_tx_alloc( unsigned int xmax, unsigned int ymax );

extern void giet_nic_rx_start( unsigned int channel );

extern void giet_nic_tx_start( unsigned int channel );

extern void giet_nic_rx_move( unsigned int channel, void* buffer );

extern void giet_nic_tx_move( unsigned int channel, void* buffer );

extern void giet_nic_rx_stop( unsigned int channel );

extern void giet_nic_tx_stop( unsigned int channel );

extern void giet_nic_rx_stats( unsigned int channel );

extern void giet_nic_tx_stats( unsigned int channel );

extern void giet_nic_rx_clear( unsigned int channel );

extern void giet_nic_tx_clear( unsigned int channel );

//////////////////////////////////////////////////////////////////////////
//               FAT related system calls 
//////////////////////////////////////////////////////////////////////////

extern int giet_fat_open( char*        pathname,
                          unsigned int flags );

extern int giet_fat_close( unsigned int fd_id );

extern int giet_fat_file_info( unsigned int     fd_id,
                               fat_file_info_t* info );

extern int giet_fat_read( unsigned int fd_id,
                          void*        buffer,
                          unsigned int count );

extern int giet_fat_write( unsigned int fd,
                           void*        buffer,
                           unsigned int count );

extern int giet_fat_lseek( unsigned int fd,
                           unsigned int offset,
                           unsigned int whence );

extern int giet_fat_remove( char*        pathname,
                            unsigned int should_be_dir );

extern int giet_fat_rename( char*  old_path,
                            char*  new_path ); 

extern int giet_fat_mkdir( char* pathname );

extern int giet_fat_opendir( char* pathname );

extern int giet_fat_closedir( unsigned int fd_id );

extern int giet_fat_readdir( unsigned int  fd_id,
                             fat_dirent_t* entry );

//////////////////////////////////////////////////////////////////////////
//                    Miscelaneous system calls
//////////////////////////////////////////////////////////////////////////

extern void giet_procs_number( unsigned int* x_size,
                               unsigned int* y_size,
                               unsigned int* nprocs );

extern void giet_vobj_get_vbase( char*         vspace_name, 
                                 char*         vobj_name, 
                                 unsigned int* vobj_vaddr);

extern void giet_vobj_get_length( char*         vspace_name, 
                                  char*         vobj_name, 
                                  unsigned int* vobj_vaddr);

extern void giet_heap_info( unsigned int* vaddr, 
                            unsigned int* length,
                            unsigned int  x,
                            unsigned int  y );

extern void giet_get_xy( void*          ptr, 
                         unsigned int*  px,
                         unsigned int*  py );

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

