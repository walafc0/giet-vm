///////////////////////////////////////////////////////////////////////////////////
// File     : nic_driver.h
// Date     : 01/11/2013
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The nic_driver.c and nic_driver.h files are part ot the GIET-VM nano-kernel.
// This driver supports the vci_multi_nic component.
// 
// It can exist only one network controller in the architecture, but this
// component supports several channels.
//
// It can be accessed directly by software with memcpy(),
// or it can be accessed through the vci_chbuf_dma component:
//  
// The '_nic_sync_write' and '_nic_sync_read' functions use a memcpy strategy to
// implement the transfer between a data buffer (user space) and the NIC
// buffer (kernel space). They are blocking until completion of the transfer.
//
// The _nic_cma_start() and _nic_cma_stop() functions use the VciChbufDma component 
// to transfer a flow of packets from the NIC RX hard chbuf (two containers) 
// to an user RX chbuf (two containers), and to transfer another flow of packets
// from an user TX chbuf (two containers) to the NIC TX chbuf (two containers).
// One NIC channel and two CMA channels must be allocated to the task 
// in the mapping_info data structure.
//
// All these access functions return -1 in case of error.
//
// The SEG_NIC_BASE address must be defined in the hard_config.h file.
//////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_NIC_DRIVERS_H_
#define _GIET_NIC_DRIVERS_H_

#include <giet_config.h>

///////////////////////////////////////////////////////////////////////////////////
//           Global Addressable Registers
///////////////////////////////////////////////////////////////////////////////////

enum SoclibMultiNicHyperRegisters {
    NIC_G_VIS                        = 0,   // bitfield : bit N = 0 -> channel N disabled
    NIC_G_ON                         = 1,   // boolean : NIC component activated
    NIC_G_NB_CHAN                    = 2,   // Number of channels (read only)
    NIC_G_BC_ENABLE                  = 3,   // boolean : Enable Broadcast if non zero
    NIC_G_TDM_ENABLE                 = 4,   // boolean : TDM Scheduler if non zero
    NIC_G_TDM_PERIOD                 = 5,   // TDM time slot value
    NIC_G_BYPASS_ENABLE              = 6,   // boolean : Enable bypass for TX packets
    // alignment
    NIC_G_MAC_4                      = 8,   // channel mac address 32 LSB bits array[8]
    NIC_G_MAC_2                      = 16,  // channel mac address 16 MSB bits array[8]
    // alignment 
    NIC_G_NPKT_RX_G2S_RECEIVED       = 32,  // number of packets received on GMII RX port
    NIC_G_NPKT_RX_G2S_DISCARDED      = 33,  // number of RX packets discarded by RX_G2S FSM

    NIC_G_NPKT_RX_DES_SUCCESS        = 34,  // number of RX packets transmited by RX_DES FSM
    NIC_G_NPKT_RX_DES_TOO_SMALL      = 35,  // number of discarded too small RX packets (<60B)
    NIC_G_NPKT_RX_DES_TOO_BIG        = 36,  // number of discarded too big RX packets (>1514B)
    NIC_G_NPKT_RX_DES_MFIFO_FULL     = 37,  // number of discarded RX packets fifo full
    NIC_G_NPKT_RX_DES_CRC_FAIL       = 38,  // number of discarded RX packets CRC32 failure

    NIC_G_NPKT_RX_DISPATCH_RECEIVED  = 39,  // number of packets received by RX_DISPATCH FSM
    NIC_G_NPKT_RX_DISPATCH_BROADCAST = 40,  // number of broadcast RX packets received
    NIC_G_NPKT_RX_DISPATCH_DST_FAIL  = 41,  // number of discarded RX packets DST MAC not found
    NIC_G_NPKT_RX_DISPATCH_CH_FULL   = 42,  // number of discarded RX packets for channel full

    NIC_G_NPKT_TX_DISPATCH_RECEIVED  = 43,  // number of packets received by TX_DISPATCH FSM
    NIC_G_NPKT_TX_DISPATCH_TOO_SMALL = 44,  // number of discarded too small TX packets (<60B)
    NIC_G_NPKT_TX_DISPATCH_TOO_BIG   = 45,  // number of discarded too big TX packets (>1514B)
    NIC_G_NPKT_TX_DISPATCH_SRC_FAIL  = 46,  // number of discarded TX packets SRC MAC failed
    NIC_G_NPKT_TX_DISPATCH_BROADCAST = 47,  // number of broadcast TX packets received
    NIC_G_NPKT_TX_DISPATCH_BYPASS    = 48,  // number of bypassed TX->RX packets
    NIC_G_NPKT_TX_DISPATCH_TRANSMIT  = 49,  // number of transmit TX packets

    NIC_CHANNEL_SPAN                 = 0x2000,
};

//////////////////////////////////////////////////////////////////////////////////////////////
//            Channel Addressable Registers
// A buffer descriptor occupies 8 bytes (two 32 bits words):
// - the 26 LSB bits of LOW WORD contain bits[31:6] of the buffer satus paddr 
// - bits[31:26] of LOW WORD and bits[19:0] of HIGH WORD contain bits[31:6] of the buffer paddr
// - the 12 MSB bits of HIGH WORD contain the common address extension of the buffer and its status
// The buffer status occupies 64 bytes but only the last bit is useful (1 for full and 0 for empty)
// The buffer address and its status address must be 64 bytes aligned (bits[5:0] equal to 0)
///////////////////////////////////////////////////////////////////////////////////////////////

enum SoclibMultiNicChannelRegisters 
{
    NIC_RX_STS_0              = 0,   // RX_0 status (full or empty)      (Read/Write)
    NIC_RX_STS_1              = 16,  // RX_1 status (full or empty)      (Read/Write)
    NIC_TX_STS_0              = 32,  // TX_0 status (full or empty)      (Read/Write)
    NIC_TX_STS_1              = 48,  // TX_1 status (full or empty)      (Read/Write)
    NIC_RX_DESC_LO_0          = 64,  // RX_0 descriptor low word         (Read/Write)
    NIC_RX_DESC_HI_0          = 65,  // RX_0 descriptor high word        (Read/Write)
    NIC_RX_DESC_LO_1          = 66,  // RX_1 descriptor low word         (Read/Write)
    NIC_RX_DESC_HI_1          = 67,  // RX_1 descriptor high word        (Read/Write) 
    NIC_TX_DESC_LO_0          = 68,  // TX_0 descriptor low word         (Read/Write)
    NIC_TX_DESC_HI_0          = 69,  // TX_0 descriptor high word        (Read/Write) 
    NIC_TX_DESC_LO_1          = 70,  // TX_1 descriptor low word         (Read/Write)
    NIC_TX_DESC_HI_1          = 71,  // TX_1 descriptor high word        (Read/Write) 
    NIC_MAC_4                 = 72,  // channel mac address 32 LSB bits  (Read Only)
    NIC_MAC_2                 = 73,  // channel mac address 16 LSB bits  (Read Only)
    NIC_RX_RUN                = 74,  // RX packets can be received       (write_only)
    NIC_TX_RUN                = 75,  // TX packets can be transmitted    (write_only)
};


///////////////////////////////////////////////////////////////////////////////////
//              Initialization functions
///////////////////////////////////////////////////////////////////////////////////

extern unsigned int _nic_get_channel_register( unsigned int channel,
                                               unsigned int index );

extern void _nic_set_channel_register( unsigned int channel,
                                       unsigned int index,
                                       unsigned int value );

extern unsigned int _nic_get_global_register( unsigned int index );

extern void _nic_set_global_register( unsigned int index,
                                      unsigned int value );

extern int _nic_global_init( unsigned int bc_enable,
                             unsigned int bypass_enable,
                             unsigned int tdm_enable,
                             unsigned int tdm_period );

extern int _nic_channel_start( unsigned int channel,
                               unsigned int is_rx,
                               unsigned int mac4,
                               unsigned int mac2 );

extern int _nic_channel_stop( unsigned int channel,
                              unsigned int is_rx );


///////////////////////////////////////////////////////////////////////////////////
//              Interrupt Service Routines
///////////////////////////////////////////////////////////////////////////////////

extern void _nic_rx_isr( unsigned int irq_type,
                         unsigned int irq_id,
                         unsigned int channel );

extern void _nic_tx_isr( unsigned int irq_type,
                         unsigned int irq_id,
                         unsigned int channel );



#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

