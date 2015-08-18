///////////////////////////////////////////////////////////////////////////////////
// File     : mmc_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_MMC_DRIVERS_H_
#define _GIET_MMC_DRIVERS_H_

#include <hard_config.h>
#include <kernel_locks.h>
#include <kernel_malloc.h>

///////////////////////////////////////////////////////////////////////////////////
// TSAR Memory Cache configuration registers offsets and commands
///////////////////////////////////////////////////////////////////////////////////

enum SoclibMemCacheFunc
{
    MEMC_CONFIG = 0,
    MEMC_INSTRM = 1,
    MEMC_RERROR = 2,

    MEMC_FUNC_SPAN = 0x200
};

enum SoclibMemCacheConfigRegs
{
    MEMC_ADDR_LO,
    MEMC_ADDR_HI,
    MEMC_BUF_LENGTH,
    MEMC_CMD_TYPE
};

enum SoclibMemCacheConfigCmd
{
    MEMC_CMD_NOP,
    MEMC_CMD_INVAL,
    MEMC_CMD_SYNC
};

enum SoclibMemCacheInstrRegs {

    // NUMBER OF LOCAL TRANSACTIONS ON DIRECT NETWORK 

    MEMC_LOCAL_READ_LO   = 0x00,
    MEMC_LOCAL_READ_HI   = 0x01,
    MEMC_LOCAL_WRITE_LO  = 0x02,
    MEMC_LOCAL_WRITE_HI  = 0x03,
    MEMC_LOCAL_LL_LO     = 0x04,
    MEMC_LOCAL_LL_HI     = 0x05,
    MEMC_LOCAL_SC_LO     = 0x06,
    MEMC_LOCAL_SC_HI     = 0x07,
    MEMC_LOCAL_CAS_LO    = 0x08,
    MEMC_LOCAL_CAS_HI    = 0x09,

    // NUMBER OF REMOTE TRANSACTIONS ON DIRECT NETWORK 

    MEMC_REMOTE_READ_LO  = 0x10,
    MEMC_REMOTE_READ_HI  = 0x11,
    MEMC_REMOTE_WRITE_LO = 0x12,
    MEMC_REMOTE_WRITE_HI = 0x13,
    MEMC_REMOTE_LL_LO    = 0x14,
    MEMC_REMOTE_LL_HI    = 0x15,
    MEMC_REMOTE_SC_LO    = 0x16,
    MEMC_REMOTE_SC_HI    = 0x17,
    MEMC_REMOTE_CAS_LO   = 0x18,
    MEMC_REMOTE_CAS_HI   = 0x19,

    // COST OF TRANSACTIONS ON DIRECT NETWORK

    MEMC_COST_READ_LO    = 0x20,
    MEMC_COST_READ_HI    = 0x21,
    MEMC_COST_WRITE_LO   = 0x22,
    MEMC_COST_WRITE_HI   = 0x23,
    MEMC_COST_LL_LO      = 0x24,
    MEMC_COST_LL_HI      = 0x25,
    MEMC_COST_SC_LO      = 0x26,
    MEMC_COST_SC_HI      = 0x27,
    MEMC_COST_CAS_LO     = 0x28,
    MEMC_COST_CAS_HI     = 0x29,

    // NUMBER OF LOCAL TRANSACTIONS ON CC NETWORK

    MEMC_LOCAL_MUPDATE_LO  = 0x40,
    MEMC_LOCAL_MUPDATE_HI  = 0x41,
    MEMC_LOCAL_MINVAL_LO   = 0x42,
    MEMC_LOCAL_MINVAL_HI   = 0x43,
    MEMC_LOCAL_CLEANUP_LO  = 0x44,
    MEMC_LOCAL_CLEANUP_HI  = 0x45,

    // NUMBER OF REMOTE TRANSACTIONS ON CC NETWORK

    MEMC_REMOTE_MUPDATE_LO = 0x50,
    MEMC_REMOTE_MUPDATE_HI = 0x51,
    MEMC_REMOTE_MINVAL_LO  = 0x52,
    MEMC_REMOTE_MINVAL_HI  = 0x53,
    MEMC_REMOTE_CLEANUP_LO = 0x54,
    MEMC_REMOTE_CLEANUP_HI = 0x55,

    // COST OF TRANSACTIONS ON CC NETWORK

    MEMC_COST_MUPDATE_LO   = 0x60,
    MEMC_COST_MUPDATE_HI   = 0x61,
    MEMC_COST_MINVAL_LO    = 0x62,
    MEMC_COST_MINVAL_HI    = 0x63,
    MEMC_COST_CLEANUP_LO   = 0x64,
    MEMC_COST_CLEANUP_HI   = 0x65,

    // TOTAL

    MEMC_TOTAL_MUPDATE_LO  = 0x68,
    MEMC_TOTAL_MUPDATE_HI  = 0x69,
    MEMC_TOTAL_MINVAL_LO   = 0x6A,
    MEMC_TOTAL_MINVAL_HI   = 0x6B,
    MEMC_TOTAL_BINVAL_LO   = 0x6C,
    MEMC_TOTAL_BINVAL_HI   = 0x6D,
};

#define MMC_REG(func,idx) ((func<<7)|idx) 

///////////////////////////////////////////////////////////////////////////////////
// MEMC access functions (for TSAR architecture)
///////////////////////////////////////////////////////////////////////////////////

extern void _mmc_init_locks();

extern unsigned int _mmc_instrument( unsigned int x,
                                     unsigned int y,
                                     unsigned int reg );

extern void _mmc_inval( unsigned long long buf_paddr, 
                        unsigned int buf_length );

extern void _mmc_sync(  unsigned long long buf_paddr, 
                        unsigned int buf_length);

extern void _mmc_isr( unsigned int irq_type,
                      unsigned int irq_id,
                      unsigned int channel );

///////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

