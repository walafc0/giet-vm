///////////////////////////////////////////////////////////////////////////////////
// File     : hba_driver.h
// Date     : 01/11/2013
// Author   : alain greiner and zhang
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The hba_driver.c and hba_driver.h files are part ot the GIET-VM kernel.
// This driver supports the SocLib VciMultiAhci component, that is a multi-channels,
// block oriented, external storage contrôler, respecting the AHCI standard.
//
// 1. Each HBA channel define an independant physical disk, but this driver
//    supports only channel 0, because the GIET-VM uses only one physical disk.
//
// 2. This HBA component support split memory buffers (several physical
//    buffers for one single command), but this driver supports only one
//    single buffer commands.
//
// 3. The "command list" can contain up to 32 independant commands, posted
//    by different user tasks. These independant transfers are handled by
//    the HBA device. The command list being a shared structure, the driver
//    must use a lock to get a slot in the command list (except in boot mode
//    when only one processor uses the HBA device). This slot is then 
//    allocated to the task until the command is completed. 
//
// 4. This driver implements two operating mode: 
//    - In synchronous mode, the calling task poll the HBA_PXCI register to
//    detect the command completion (busy waiting). 
//    - In descheduling mode, the calling task is descheduled, and must be
//    restart when the command is completed.
// 
// 5. As several user tasks can concurrently register commands in the command
//    list, and there is only one HBA interrupt, this interrupt is not linked
//    to a specific task. In descheduling mode, the HBA IRQ is a "global" IRQ
//    that is statically routed to processor P[x_io,y_io,0] in cluster_io. 
//    The associated global HBA_ISR send a WAKUP WTI to all tasks that have
//    a completed active command. In order to know which commands expect
//    completion, the HBA_ISR uses the _hba_active_cmd table which indicates
//    currently active commands (in descheduling mode only).
//
// The SEG_IOC_BASE virtual address must be defined in the hard_config.h file.
//////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_HBA_DRIVERS_H_
#define _GIET_HBA_DRIVERS_H_

///////////////////////////////////////////////////////////////////////////////////
// HBA component registers offsets
///////////////////////////////////////////////////////////////////////////////////

enum SoclibMultiAhciRegisters 
{
  HBA_PXCLB            = 0,         // command list base address 32 LSB bits
  HBA_PXCLBU           = 1,         // command list base address 32 MSB bits
  HBA_PXIS             = 4,         // interrupt status
  HBA_PXIE             = 5,         // interrupt enable
  HBA_PXCMD            = 6,         // run
  HBA_PXCI             = 14,        // command bit-vector     
  HBA_SPAN             = 0x400,     // 4 Kbytes per channel => 1024 slots
};

///////////////////////////////////////////////////////////////////////////////////
// Data structures for command table 
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////
typedef struct hba_cmd_header_s // size = 16 bytes
{
    // WORD 0
    unsigned int        res0;       // reserved	
  
    // WORD 1
    unsigned char	    lba0;	    // LBA 7:0
    unsigned char	    lba1;	    // LBA 15:8
    unsigned char	    lba2;	    // LBA 23:16
    unsigned char	    res1;	    // reserved
  
    // WORD 2
    unsigned char	    lba3;	    // LBA 31:24
    unsigned char	    lba4;	    // LBA 39:32
    unsigned char	    lba5;	    // LBA 47:40
    unsigned char	    res2;	    // reserved
  
    // WORD 3 
    unsigned int        res3;       // reserved	

} hba_cmd_header_t;

///////////////////////////////
typedef struct hba_cmd_buffer_s  // size = 16 bytes
{
    unsigned int        dba;	    // Buffer base address 32 LSB bits
    unsigned int        dbau;	    // Buffer base address 32 MSB bits
    unsigned int        res0;	    // reserved
    unsigned int        dbc;	    // Buffer byte count

} hba_cmd_buffer_t;

//////////////////////////////
typedef struct hba_cmd_table_s  // size = 32 bytes
{

    hba_cmd_header_t   header;      // contains LBA
    hba_cmd_buffer_t   buffer;      // only one physical buffer

} hba_cmd_table_t;

///////////////////////////////////////////////////////////////////////////////////
// Data structure for command descriptor in command list
///////////////////////////////////////////////////////////////////////////////////

/////////////////////////////
typedef struct hba_cmd_desc_s  // size = 16 bytes
{
	// WORD 0
    unsigned char       flag[2];    // W in bit 6 of flag[0]
    unsigned char       prdtl[2];	// Number of buffers

    // WORD 1
    unsigned int        prdbc;		// Number of bytes actually transfered

    // WORD 2, WORD 3
    unsigned int        ctba;		// Command Table base address 32 LSB bits
    unsigned int        ctbau;		// Command Table base address 32 MSB bits

} hba_cmd_desc_t;

///////////////////////////////////////////////////////////////////////////////////
//              access functions  
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// This function initializes for a given channel
// - the HBA hardware registers,
// - the command list pointer,
// - the command lists physical addresse,
// - the command tables physical addresses array,
///////////////////////////////////////////////////////////////////////////////////
extern unsigned int _hba_init (); 

///////////////////////////////////////////////////////////////////////////////////
// This function register a command in Command List and Command Table
// for a single physical buffer, and updates the HBA_PXCI register.
// Returns 0 if success, > 0 if error.
///////////////////////////////////////////////////////////////////////////////////
extern unsigned int _hba_access( unsigned int       use_irq,
                                 unsigned int       to_mem,
                                 unsigned int       lba, 
                                 unsigned long long paddr, 
                                 unsigned int       count );

///////////////////////////////////////////////////////////////////////////////////
// Interrupt Service Routine executed in descheduling mode.
///////////////////////////////////////////////////////////////////////////////////
extern void _hba_isr( unsigned int irq_type,
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

