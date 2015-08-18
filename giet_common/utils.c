///////////////////////////////////////////////////////////////////////////
// File     : utils.c
// Date     : 18/10/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////
// The utils.c and utils.h files are part of the GIET-VM nano-kernel.
///////////////////////////////////////////////////////////////////////////

#include <utils.h>
#include <tty0.h>
#include <giet_config.h>
#include <hard_config.h>
#include <mapping_info.h>
#include <tty_driver.h>
#include <ctx_handler.h>

// This variable is allocated in the boot.c file or in kernel_init.c file
extern static_scheduler_t* _schedulers[X_SIZE][Y_SIZE][NB_PROCS_MAX];

///////////////////////////////////////////////////////////////////////////
//         CP0 registers access functions
///////////////////////////////////////////////////////////////////////////

/////////////////////////
unsigned int _get_sched() 
{
    unsigned int ret;
    asm volatile( "mfc0      %0,     $4,2    \n" 
                  : "=r"(ret) );
    return ret;
}
///////////////////////
unsigned int _get_epc() 
{
    unsigned int ret;
    asm volatile( "mfc0      %0,    $14     \n"
                  : "=r"(ret) );
    return ret;
}
////////////////////////
unsigned int _get_bvar() 
{
    unsigned int ret;
    asm volatile( "mfc0      %0,    $8     \n"
                  : "=r"(ret));
    return ret;
}
//////////////////////
unsigned int _get_cr() 
{
    unsigned int ret;
    asm volatile( "mfc0      %0,    $13    \n"
                  : "=r"(ret));
    return ret;
}
//////////////////////
unsigned int _get_sr() 
{
    unsigned int ret;
    asm volatile( "mfc0      %0,     $12   \n"
                  : "=r"(ret));
    return ret;
}
//////////////////////////
unsigned int _get_procid() 
{
    unsigned int ret;
    asm volatile ( "mfc0     %0,     $15, 1  \n"
                   :"=r" (ret) );
    return (ret & 0xFFF);
}
////////////////////////////
unsigned int _get_proctime() 
{
    unsigned int ret;
    asm volatile ( "mfc0     %0,     $9      \n"
                   :"=r" (ret) );
    return ret;
}

/////////////////////////////////////////////
void _it_disable( unsigned int * save_sr_ptr) 
{
    unsigned int sr = 0;
    asm volatile( "li      $3,        0xFFFFFFFE    \n"
                  "mfc0    %0,        $12           \n"
                  "and     $3,        $3,   %0      \n"  
                  "mtc0    $3,        $12           \n" 
                  : "+r"(sr)
                  :
                  : "$3" );
    *save_sr_ptr = sr;
}
//////////////////////////////////////////////
void _it_restore( unsigned int * save_sr_ptr ) 
{
    unsigned int sr = *save_sr_ptr;
    asm volatile( "mtc0    %0,        $12           \n" 
                  :
                  : "r"(sr)
                  : "memory" );
}

/////////////////////////////////
void _set_sched(unsigned int val) 
{
    asm volatile ( "mtc0     %0,     $4, 2          \n"
                   :
                   :"r" (val) );
}
//////////////////////////////
void _set_sr(unsigned int val) 
{
    asm volatile ( "mtc0     %0,     $12            \n"
                   :
                   :"r" (val) );
}


///////////////////////////////////////////////////////////////////////////
//         CP2 registers access functions
///////////////////////////////////////////////////////////////////////////

////////////////////////////
unsigned int _get_mmu_ptpr() 
{
    unsigned int ret;
    asm volatile( "mfc2      %0,     $0      \n"
                  : "=r"(ret) );
    return ret;
}
////////////////////////////
unsigned int _get_mmu_mode() 
{
    unsigned int ret;
    asm volatile( "mfc2      %0,     $1      \n"
                  : "=r"(ret) );
    return ret;
}
////////////////////////////////////
void _set_mmu_ptpr(unsigned int val) 
{
    asm volatile ( "mtc2     %0,     $0      \n"
                   :
                   :"r" (val)
                   :"memory" );
}
////////////////////////////////////
void _set_mmu_mode(unsigned int val) 
{
    asm volatile ( "mtc2     %0,     $1      \n"
                   :
                   :"r" (val)
                   :"memory" );
}
////////////////////////////////////////////
void _set_mmu_dcache_inval(unsigned int val) 
{
    asm volatile ( "mtc2     %0,     $7      \n"
                   :
                   :"r" (val)
                   :"memory" );
}


///////////////////////////////////////////////////////////////////////////
//          Physical addressing related functions
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////
unsigned int _physical_read( unsigned long long paddr ) 
{
    unsigned int value;
    unsigned int lsb = (unsigned int) paddr;
    unsigned int msb = (unsigned int) (paddr >> 32);
    unsigned int sr;

    _it_disable(&sr);

    asm volatile( "mfc2   $2,     $1            \n"  /* $2 <= MMU_MODE   */
                  "andi   $3,     $2,     0xb   \n"
                  "mtc2   $3,     $1            \n"  /* DTLB off         */    

                  "mtc2   %2,     $24           \n"  /* PADDR_EXT <= msb */   
                  "lw     %0,     0(%1)         \n"  /* value <= *paddr  */
                  "mtc2   $0,     $24           \n"  /* PADDR_EXT <= 0   */   

                  "mtc2   $2,     $1            \n"  /* restore MMU_MODE */
                  : "=r" (value)
                  : "r" (lsb), "r" (msb)
                  : "$2", "$3" );

    _it_restore(&sr);
    return value;
}
////////////////////////////////////////////////
void _physical_write( unsigned long long paddr, 
                      unsigned int       value ) 
{
    unsigned int lsb = (unsigned int)paddr;
    unsigned int msb = (unsigned int)(paddr >> 32);
    unsigned int sr;

   _it_disable(&sr);

    asm volatile( "mfc2   $2,     $1           \n"  /* $2 <= MMU_MODE   */
                  "andi   $3,     $2,    0xb   \n"
                  "mtc2   $3,     $1           \n"  /* DTLB off         */    

                  "mtc2   %2,     $24          \n"  /* PADDR_EXT <= msb */   
                  "sw     %0,     0(%1)        \n"  /* *paddr <= value  */
                  "mtc2   $0,     $24          \n"  /* PADDR_EXT <= 0   */   

                  "mtc2   $2,     $1           \n"  /* restore MMU_MODE */
                  "sync                        \n"
                  :
                  : "r" (value), "r" (lsb), "r" (msb)
                  : "$2", "$3" );

    _it_restore(&sr);
}

/////////////////////////////////////////////////////////////////
unsigned long long _physical_read_ull( unsigned long long paddr ) 
{
    unsigned int data_lsb;
    unsigned int data_msb;
    unsigned int addr_lsb = (unsigned int) paddr;
    unsigned int addr_msb = (unsigned int) (paddr >> 32);
    unsigned int sr;

    _it_disable(&sr);

    asm volatile( "mfc2   $2,     $1           \n"  /* $2 <= MMU_MODE       */
                  "andi   $3,     $2,    0xb   \n"
                  "mtc2   $3,     $1           \n"  /* DTLB off             */    

                  "mtc2   %3,     $24          \n"  /* PADDR_EXT <= msb     */   
                  "lw     %0,     0(%2)        \n"  /* data_lsb <= *paddr   */
                  "lw     %1,     4(%2)        \n"  /* data_msb <= *paddr+4 */
                  "mtc2   $0,     $24          \n"  /* PADDR_EXT <= 0       */   

                  "mtc2   $2,     $1           \n"  /* restore MMU_MODE     */
                  : "=r" (data_lsb), "=r"(data_msb)
                  : "r" (addr_lsb), "r" (addr_msb)
                  : "$2", "$3" );

    _it_restore(&sr);

    return ( (((unsigned long long)data_msb)<<32) +
             (((unsigned long long)data_lsb)) );
}

///////////////////////////////////////////////////
void _physical_write_ull( unsigned long long paddr, 
                          unsigned long long value ) 
{
    unsigned int addr_lsb = (unsigned int)paddr;
    unsigned int addr_msb = (unsigned int)(paddr >> 32);
    unsigned int data_lsb = (unsigned int)value;
    unsigned int data_msb = (unsigned int)(value >> 32);
    unsigned int sr;

    _it_disable(&sr);

    asm volatile( "mfc2   $2,     $1           \n"  /* $2 <= MMU_MODE     */
                  "andi   $3,     $2,    0xb   \n"
                  "mtc2   $3,     $1           \n"  /* DTLB off           */    

                  "mtc2   %3,     $24          \n"  /* PADDR_EXT <= msb   */   
                  "sw     %0,     0(%2)        \n"  /* *paddr <= value    */
                  "sw     %1,     4(%2)        \n"  /* *paddr+4 <= value  */
                  "mtc2   $0,     $24          \n"  /* PADDR_EXT <= 0     */   

                  "mtc2   $2,     $1           \n"  /* restore MMU_MODE   */
                  "sync                        \n"
                  :
                  : "r"(data_lsb),"r"(data_msb),"r"(addr_lsb),"r"(addr_msb)
                  : "$2", "$3" );

    _it_restore(&sr);
}

////////////////////////////////////////////////////
void _physical_memcpy( unsigned long long dst_paddr,  // dest buffer paddr
                       unsigned long long src_paddr,  // source buffer paddr
                       unsigned int size )            // bytes
{
    // check alignment constraints
    if ( (dst_paddr & 3) || (src_paddr & 3) || (size & 3) ) 
    {
        _puts("\n[GIET ERROR] in _physical_memcpy() : buffer unaligned\n");
        _exit();
    }

    unsigned int src_lsb = (unsigned int)src_paddr;
    unsigned int src_msb = (unsigned int)(src_paddr >> 32);
    unsigned int dst_lsb = (unsigned int)dst_paddr;
    unsigned int dst_msb = (unsigned int)(dst_paddr >> 32);
    unsigned int iter    = size>>2;
    unsigned int data;
    unsigned int sr;

    _it_disable(&sr);

    asm volatile( "mfc2   $2,     $1         \n" /* $2 <= current MMU_MODE */
                  "andi   $3,     $2,   0xb  \n" /* $3 <= new MMU_MODE     */
                  "mtc2   $3,     $1         \n" /* DTLB off               */    

                  "move   $4,     %5         \n" /* $4 < iter              */
                  "move   $5,     %1         \n" /* $5 < src_lsb           */
                  "move   $6,     %3         \n" /* $6 < src_lsb           */

                  "ph_memcpy_loop:           \n"
                  "mtc2   %2,     $24        \n" /* PADDR_EXT <= src_msb   */   
                  "lw     %0,     0($5)      \n" /* data <= *src_paddr     */
                  "mtc2   %4,     $24        \n" /* PADDR_EXT <= dst_msb   */   
                  "sw     %0,     0($6)      \n" /* *dst_paddr <= data     */

                  "addi   $4,     $4,   -1   \n" /* iter = iter - 1        */
                  "addi   $5,     $5,   4    \n" /* src_lsb += 4           */
                  "addi   $6,     $6,   4    \n" /* dst_lsb += 4           */
                  "bne    $4,     $0, ph_memcpy_loop \n"
                  "nop                       \n"

                  "mtc2   $0,     $24        \n" /* PADDR_EXT <= 0         */   
                  "mtc2   $2,     $1         \n" /* restore MMU_MODE       */
                  : "=r" (data)
                  : "r"(src_lsb),"r"(src_msb),"r"(dst_lsb),
                    "r"(dst_msb), "r"(iter)
                  : "$2", "$3", "$4", "$5", "$6" );

    _it_restore(&sr);
} // end _physical_memcpy()

////////////////////////////////////////////////
void _physical_memset( unsigned long long paddr,     // dest buffer paddr
                       unsigned int       size,      // bytes
                       unsigned int       data )     // written value
{
    // check alignment constraints
    if ( (paddr & 3) || (size & 7) )
    {
        _puts("\n[GIET ERROR] in _physical_memset() : buffer unaligned\n");
        _exit();
    }

    unsigned int lsb  = (unsigned int)paddr;
    unsigned int msb  = (unsigned int)(paddr >> 32);
    unsigned int sr;

    _it_disable(&sr);

    asm volatile( "mfc2   $8,     $1         \n" /* $8 <= current MMU_MODE */
                  "andi   $9,     $8,   0xb  \n" /* $9 <= new MMU_MODE     */
                  "mtc2   $9,     $1         \n" /* DTLB off               */
                  "mtc2   %3,     $24        \n" /* PADDR_EXT <= msb       */

                  "1:                        \n" /* set 8 bytes per iter   */
                  "sw     %2,     0(%0)      \n" /* *src_paddr     = data  */
                  "sw     %2,     4(%0)      \n" /* *(src_paddr+4) = data  */
                  "addi   %1,     %1,   -8   \n" /* size -= 8              */
                  "addi   %0,     %0,    8   \n" /* src_paddr += 8         */
                  "bnez   %1,     1b         \n" /* loop while size != 0   */

                  "mtc2   $0,     $24        \n" /* PADDR_EXT <= 0         */
                  "mtc2   $8,     $1         \n" /* restore MMU_MODE       */
                  : "+r"(lsb), "+r"(size)
                  : "r"(data), "r" (msb)
                  : "$8", "$9", "memory" );

    _it_restore(&sr);
}  // _physical_memset()

///////////////////////////////////////////////
void _io_extended_write( unsigned int*  vaddr,
                         unsigned int   value )
{
    unsigned long long paddr;

    if ( _get_mmu_mode() & 0x4 )  // MMU activated : use virtual address
    {
        *vaddr = value;
    }
    else                          // use paddr extension for IO
    {
        paddr = (unsigned long long)(unsigned int)vaddr +
                (((unsigned long long)((X_IO<<Y_WIDTH) + Y_IO))<<32); 
        _physical_write( paddr, value );
    }
    asm volatile("sync" ::: "memory");
}

//////////////////////////////////////////////////////
unsigned int _io_extended_read( unsigned int*  vaddr )
{
    unsigned long long paddr;

    if ( _get_mmu_mode() & 0x4 )  // MMU activated : use virtual address
    {
        return *(volatile unsigned int*)vaddr;
    }
    else                          // use paddr extension for IO
    {
        paddr = (unsigned long long)(unsigned int)vaddr +
                (((unsigned long long)((X_IO<<Y_WIDTH) + Y_IO))<<32); 
        return _physical_read( paddr );
    }
}

////////////////////////////////////////////////////////////////////////////
//           Scheduler and tasks context access functions
////////////////////////////////////////////////////////////////////////////


///////////////////////////////////
unsigned int _get_current_task_id() 
{
    static_scheduler_t * psched = (static_scheduler_t *) _get_sched();
    return (unsigned int) (psched->current);
}

////////////////////////////////////////////
unsigned int _get_task_slot( unsigned int x,
                             unsigned int y,
                             unsigned int p,
                             unsigned int ltid,
                             unsigned int slot )
{
    static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
    return psched->context[ltid][slot];
}

////////////////////////////////////
void _set_task_slot( unsigned int x,
                     unsigned int y,
                     unsigned int p,
                     unsigned int ltid,
                     unsigned int slot,
                     unsigned int value )
{
    static_scheduler_t* psched  = (static_scheduler_t*)_schedulers[x][y][p];
    psched->context[ltid][slot] = value;
}

///////////////////////////////////////////////////
unsigned int _get_context_slot( unsigned int slot )
{
    static_scheduler_t* psched  = (static_scheduler_t*)_get_sched();
    unsigned int        task_id = psched->current;
    return psched->context[task_id][slot];
}

///////////////////////////////////////////
void _set_context_slot( unsigned int slot,
                       unsigned int value )
{
    static_scheduler_t* psched  = (static_scheduler_t*)_get_sched();
    unsigned int        task_id = psched->current;
    psched->context[task_id][slot] = value;
}

/////////////////////////////////////////////////////////////////////////////
//      Access functions to mapping_info data structure
/////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
mapping_cluster_t * _get_cluster_base(mapping_header_t * header) 
{
    return (mapping_cluster_t *) ((char *) header +
            MAPPING_HEADER_SIZE);
}
//////////////////////////////////////////////////////////
mapping_pseg_t * _get_pseg_base(mapping_header_t * header) 
{
    return (mapping_pseg_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * X_SIZE * Y_SIZE);
}
//////////////////////////////////////////////////////////////
mapping_vspace_t * _get_vspace_base(mapping_header_t * header) 
{
    return (mapping_vspace_t *)  ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * X_SIZE * Y_SIZE +
            MAPPING_PSEG_SIZE * header->psegs);
}
//////////////////////////////////////////////////////////
mapping_vseg_t * _get_vseg_base(mapping_header_t * header)
{
    return (mapping_vseg_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * X_SIZE * Y_SIZE +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces);
}
//////////////////////////////////////////////////////////
mapping_task_t * _get_task_base(mapping_header_t * header) 
{
    return (mapping_task_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * X_SIZE * Y_SIZE +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces +
            MAPPING_VSEG_SIZE * header->vsegs);
}
/////////////////////////////////////////////////////////
mapping_proc_t *_get_proc_base(mapping_header_t * header) 
{
    return (mapping_proc_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * X_SIZE * Y_SIZE +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces +
            MAPPING_VSEG_SIZE * header->vsegs +
            MAPPING_TASK_SIZE * header->tasks);
}
///////////////////////////////////////////////////////
mapping_irq_t *_get_irq_base(mapping_header_t * header) 
{
    return (mapping_irq_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * X_SIZE * Y_SIZE +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces +
            MAPPING_VSEG_SIZE * header->vsegs +
            MAPPING_TASK_SIZE * header->tasks +
            MAPPING_PROC_SIZE * header->procs);
}
///////////////////////////////////////////////////////////////
mapping_periph_t *_get_periph_base(mapping_header_t * header) 
{
    return (mapping_periph_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * X_SIZE * Y_SIZE +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces +
            MAPPING_VSEG_SIZE * header->vsegs +
            MAPPING_TASK_SIZE * header->tasks +
            MAPPING_PROC_SIZE * header->procs +
            MAPPING_IRQ_SIZE * header->irqs);
}

///////////////////////////////////////////////////////////////////////////
//             Miscelaneous functions
///////////////////////////////////////////////////////////////////////////

//////////////////////////////////////
__attribute__((noreturn)) void _exit() 
{
    unsigned int procid = _get_procid();
    unsigned int x      = (procid >> (Y_WIDTH + P_WIDTH)) & ((1<<X_WIDTH)-1);
    unsigned int y      = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
    unsigned int lpid   = procid & ((1<<P_WIDTH)-1);


    _puts("\n[GIET PANIC] processor[");
    _putd( x );
    _puts(",");
    _putd( y );
    _puts(",");
    _putd( lpid );
    _puts("] exit at cycle ");
    _putd( _get_proctime() );
    _puts(" ...\n");

    while (1) { asm volatile ("nop"); }
}

/////////////////////////////////////
void _random_wait( unsigned int val )
{
    unsigned int mask  = (1<<(val&0x1F))-1;
    unsigned int delay = (_get_proctime() ^ (_get_procid()<<4)) & mask;
    asm volatile( "move  $3,   %0                 \n"
                  "loop_nic_completed:            \n"
                  "nop                            \n"
                  "addi  $3,   $3, -1             \n"
                  "bnez  $3,   loop_nic_completed \n"
                  "nop                            \n"
                  :
                  : "r" (delay)
                  : "$3" ); 
}

///////////////////////////
void _break( char* string ) 
{
    char byte;

    _puts("\n[GIET DEBUG] break from ");
    _puts( string );
    _puts(" / stoke any key to continue\n");
    _getc( &byte );
}

////////////////////////////////////
unsigned int _strlen( char* string )
{
    unsigned int i = 0;
    while ( string[i] != 0 ) i++;
    return i;
}

///////////////////////////////////////
unsigned int _strcmp( const char * s1,
                      const char * s2 )
{
    while (1)
    {
        if (*s1 != *s2) return 1;
        if (*s1 == 0)   break;
        s1++, s2++;
    }
    return 0;
}

///////////////////////////////////////
unsigned int _strncmp( const char * s1, 
                       const char * s2, 
                       unsigned int n ) 
{
    unsigned int i;
    for (i = 0; i < n; i++) 
    {
        if (s1[i] != s2[i])  return 1; 
        if (s1[i] == 0)      break;
    }
    return 0;
}

/////////////////////////////////////////
char* _strcpy( char* dest, char* source )
{
    if (!dest || !source) return dest;

    while (*source)
    {
        *(dest) = *(source);
        dest++;
        source++;
    }
    *dest = 0;
    return dest;
}

/////////////////////////////////////////////////////
void _dcache_buf_invalidate( unsigned int buf_vbase, 
                             unsigned int buf_size ) 
{
    unsigned int offset;
    unsigned int tmp;
    unsigned int line_size;   // bytes

    // compute data cache line size based on config register (bits 12:10)
    asm volatile(
                 "mfc0 %0, $16, 1" 
                 : "=r" (tmp) );

    tmp = ((tmp >> 10) & 0x7);
    line_size = 2 << tmp;

    // iterate on cache lines 
    for ( offset = 0; offset < buf_size; offset += line_size) 
    {
        _set_mmu_dcache_inval( buf_vbase + offset );
    }
}



/////////////////////////////////////////////
void _get_sqt_footprint( unsigned int* width,
                         unsigned int* heigth,
                         unsigned int* levels )
{
    mapping_header_t*   header  = (mapping_header_t *)SEG_BOOT_MAPPING_BASE;
    mapping_cluster_t*  cluster = _get_cluster_base(header);

    unsigned int x;
    unsigned int y;
    unsigned int cid;
    unsigned int w = 0;
    unsigned int h = 0;

    // scan all clusters to compute SQT footprint (w,h)
    for ( x = 0 ; x < X_SIZE ; x++ )
    {
        for ( y = 0 ; y < Y_SIZE ; y++ )
        {
            cid = x * Y_SIZE + y;
            if ( cluster[cid].procs )  // cluster contains processors
            {
                if ( x > w ) w = x;
                if ( y > h ) h = y;
            }
        }
    }           
    *width  = w + 1;
    *heigth = h + 1;
    
    // compute SQT levels
    unsigned int z = (h > w) ? h : w;
    *levels = (z < 1) ? 1 : (z < 2) ? 2 : (z < 4) ? 3 : (z < 8) ? 4 : 5;
}
     


///////////////////////////////////////////////////////////////////////////
//   Required by GCC
///////////////////////////////////////////////////////////////////////////

////////////////////////////////
void* memcpy( void*        dest,     // dest buffer vbase
              const void*  source,   // source buffer vbase
              unsigned int size )    // bytes
{
    unsigned int* idst = (unsigned int*)dest;
    unsigned int* isrc = (unsigned int*)source;

    // word-by-word copy
    if (!((unsigned int) idst & 3) && !((unsigned int) isrc & 3)) 
    {
        while (size > 3) 
        {
            *idst++ = *isrc++;
            size -= 4;
        }
    }

    unsigned char* cdst = (unsigned char*)dest;
    unsigned char* csrc = (unsigned char*)source;

    /* byte-by-byte copy */
    while (size--) 
    {
        *cdst++ = *csrc++;
    }
    return dest;
}

/////////////////////////////////
void* memset( void*        dst, 
              int          value, 
              unsigned int count ) 
{
    // word-by-word copy
    unsigned int* idst = dst;
    unsigned int  data = (((unsigned char)value)      ) |
                         (((unsigned char)value) <<  8) |
                         (((unsigned char)value) << 16) |
                         (((unsigned char)value) << 24) ;

    if ( ! ((unsigned int)idst & 3) )
    {
        while ( count > 3 )
        {
            *idst++ = data;
            count -= 4;
        }
    }
   
    // byte-by-byte copy
    unsigned char* cdst = dst;
    while (count--) 
    {
        *cdst++ = (unsigned char)value;
    }
    return dst;
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

