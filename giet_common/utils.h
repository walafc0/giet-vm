///////////////////////////////////////////////////////////////////////////////////
// File     : utils.h
// Date     : 18/10/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The utils.c and utils.h files are part of the GIET-VM nano-kernel.
// They define various utility functions, that can be used by both the 
// boot code and the kernel code (but not by the user applications).
///////////////////////////////////////////////////////////////////////////////////

#ifndef GIET_UTILS_H
#define GIET_UTILS_H

#include <mapping_info.h>

//////////////////////////////////////////////////////////////////////////////////
// NULL pointer definition
//////////////////////////////////////////////////////////////////////////////////

#define NULL (void *)0

///////////////////////////////////////////////////////////////////////////////////
// To access the virtual addresses defined in the giet_vsegs.ld file.
///////////////////////////////////////////////////////////////////////////////////

typedef struct _ld_symbol_s _ld_symbol_t;

extern _ld_symbol_t boot_code_vbase;
extern _ld_symbol_t boot_data_vbase;

extern _ld_symbol_t kernel_code_vbase;
extern _ld_symbol_t kernel_data_vbase;
extern _ld_symbol_t kernel_uncdata_vbase;
extern _ld_symbol_t kernel_init_vbase;



///////////////////////////////////////////////////////////////////////////
//     CP0 registers access functions
///////////////////////////////////////////////////////////////////////////

extern unsigned int _get_sched(void);

extern unsigned int _get_epc(void);

extern unsigned int _get_bvar(void);

extern unsigned int _get_cr(void);

extern unsigned int _get_sr(void);

extern unsigned int _get_procid(void);

extern unsigned int _get_proctime(void);

extern void         _it_disable( unsigned int* save_sr_ptr );

extern void         _it_restore( unsigned int* save_sr_ptr );

extern void         _set_sched(unsigned int value);

extern void         _set_sr(unsigned int value);

///////////////////////////////////////////////////////////////////////////
//     CP2 registers access functions
///////////////////////////////////////////////////////////////////////////

extern unsigned int _get_mmu_ptpr(void);

extern unsigned int _get_mmu_mode(void);

extern void         _set_mmu_ptpr(unsigned int value);

extern void         _set_mmu_mode(unsigned int value);

extern void         _set_mmu_dcache_inval(unsigned int value);

///////////////////////////////////////////////////////////////////////////
//     Physical addressing functions
///////////////////////////////////////////////////////////////////////////

extern unsigned int _physical_read(  unsigned long long paddr );

extern void         _physical_write( unsigned long long paddr,
                                     unsigned int       value );

extern unsigned long long _physical_read_ull(  unsigned long long paddr );

extern void               _physical_write_ull( unsigned long long paddr,
                                               unsigned long long value );

extern void         _physical_memcpy( unsigned long long dst_paddr,
                                      unsigned long long src_paddr,
                                      unsigned int       size );

extern void         _physical_memset( unsigned long long buf_paddr, 
                                      unsigned int       size, 
                                      unsigned int       data );

extern unsigned int _io_extended_read(  unsigned int* vaddr );

extern void         _io_extended_write( unsigned int* vaddr,
                                        unsigned int  value );

///////////////////////////////////////////////////////////////////////////
//       Scheduler and task context access functions
///////////////////////////////////////////////////////////////////////////

extern unsigned int _get_current_task_id(void);

extern unsigned int _get_task_slot( unsigned int x,
                                    unsigned int y,
                                    unsigned int p,
                                    unsigned int ltid,
                                    unsigned int slot );

extern void         _set_task_slot( unsigned int x,
                                    unsigned int y,
                                    unsigned int p,
                                    unsigned int ltid,
                                    unsigned int slot,
                                    unsigned int value );

extern unsigned int _get_context_slot( unsigned int slot );

extern void         _set_context_slot( unsigned int slot,
                                       unsigned int value );

///////////////////////////////////////////////////////////////////////////
//     Mapping access functions
///////////////////////////////////////////////////////////////////////////

extern mapping_cluster_t *  _get_cluster_base(mapping_header_t* header);
extern mapping_pseg_t *     _get_pseg_base(mapping_header_t* header);
extern mapping_vspace_t *   _get_vspace_base(mapping_header_t* header);
extern mapping_vseg_t *     _get_vseg_base(mapping_header_t* header);
extern mapping_task_t *     _get_task_base(mapping_header_t* header);
extern mapping_proc_t *     _get_proc_base(mapping_header_t* header);
extern mapping_irq_t *      _get_irq_base(mapping_header_t* header);
extern mapping_periph_t *   _get_periph_base(mapping_header_t* header);

///////////////////////////////////////////////////////////////////////////
//     Miscelaneous functions
///////////////////////////////////////////////////////////////////////////

extern void         _exit(void);

extern void         _random_wait( unsigned int value );

extern void         _break( char* str);

extern unsigned int _strlen( char* str);

extern unsigned int _strcmp(const char* s1,
                            const char* s2);

extern unsigned int _strncmp(const char*  s1, 
                             const char*  s2, 
                             unsigned int n);

extern char*        _strcpy( char*        dest,
                             char*        source );

extern void         _dcache_buf_invalidate( unsigned int buf_vbase, 
                                            unsigned int buf_size );

extern void         _get_sqt_footprint( unsigned int* width,
                                        unsigned int* heigth,
                                        unsigned int* levels );

//////////////////////////////////////////////////////////////////////////
//     Required by GCC
//////////////////////////////////////////////////////////////////////////

extern void* memcpy( void*        dst, 
                     const void*  src, 
                     unsigned int size );

extern void* memset( void*        dst, 
                     int          value, 
                     unsigned int count );


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

