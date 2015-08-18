///////////////////////////////////////////////////////////////////////////////////
// File     : exc_handler.c
// Date     : 01/04/2012
// Author   : alain greiner and joel porquet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <exc_handler.h>
#include <ctx_handler.h>
#include <sys_handler.h>
#include <utils.h>
#include <tty0.h>

///////////////////////////////////////////////////////////////////////////////////
// Prototypes of exception handlers.
///////////////////////////////////////////////////////////////////////////////////

static void _cause_ukn();
static void _cause_adel();
static void _cause_ades();
static void _cause_ibe();
static void _cause_dbe();
static void _cause_bp();
static void _cause_ri();
static void _cause_cpu();
static void _cause_ovf();

extern void _int_handler();
extern void _sys_handler();

///////////////////////////////////////////////////////////////////////////////////
// Initialize the exception vector indexed by the CR XCODE field
///////////////////////////////////////////////////////////////////////////////////

__attribute__((section(".kdata")))
const _exc_func_t _cause_vector[16] = 
{
    &_int_handler,  /* 0000 : external interrupt */
    &_cause_ukn,    /* 0001 : undefined exception */
    &_cause_ukn,    /* 0010 : undefined exception */
    &_cause_ukn,    /* 0011 : undefined exception */
    &_cause_adel,   /* 0100 : illegal address read exception */
    &_cause_ades,   /* 0101 : illegal address write exception */
    &_cause_ibe,    /* 0110 : instruction bus error exception */
    &_cause_dbe,    /* 0111 : data bus error exception */
    &_sys_handler,  /* 1000 : system call */
    &_cause_bp,     /* 1001 : breakpoint exception */
    &_cause_ri,     /* 1010 : illegal codop exception */
    &_cause_cpu,    /* 1011 : illegal coprocessor access */
    &_cause_ovf,    /* 1100 : arithmetic overflow exception */
    &_cause_ukn,    /* 1101 : undefined exception */
    &_cause_ukn,    /* 1110 : undefined exception */
    &_cause_ukn,    /* 1111 : undefined exception */
};

///////////////////////////////////////////////
static void _display_cause( unsigned int type ) 
{
    unsigned int gpid       = _get_procid();
    unsigned int cluster_xy = gpid >> P_WIDTH;
    unsigned int x          = cluster_xy >> Y_WIDTH;
    unsigned int y          = cluster_xy & ((1<<Y_WIDTH)-1);
    unsigned int lpid       = gpid & ((1<<P_WIDTH)-1);

    unsigned int task       = _get_context_slot(CTX_LTID_ID);


    const char * mips32_exc_str[] = { "strange unknown cause  ",
                                      "illegal read address   ",
                                      "illegal write address  ",
                                      "inst bus error         ",
                                      "data bus error         ",
                                      "breakpoint             ",
                                      "reserved instruction   ",
                                      "illegal coproc access  ",
                                      "arithmetic overflow    "};

    _printf("\n[GIET] Exception for task %d on processor[%d,%d,%d] at cycle %d\n"
            " - type      : %s\n"
            " - EPC       : %x\n"
            " - BVAR      : %x\n"
            "...Task desactivated\n",
            task, x, y, lpid, _get_proctime(),
            mips32_exc_str[type], _get_epc(), _get_bvar() );

    // goes to sleeping state
    _set_context_slot( CTX_NORUN_ID , 1 );

    // deschedule 
    unsigned int save_sr;  
    _it_disable( &save_sr );
    _ctx_switch();

}  // end display_cause()

static void _cause_ukn()  { _display_cause(0); }
static void _cause_adel() { _display_cause(1); }
static void _cause_ades() { _display_cause(2); }
static void _cause_ibe()  { _display_cause(3); }
static void _cause_dbe()  { _display_cause(4); }
static void _cause_bp()   { _display_cause(5); }
static void _cause_ri()   { _display_cause(6); }
static void _cause_cpu()  { _display_cause(7); }
static void _cause_ovf()  { _display_cause(8); }

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

