/****************************************************************************/
/*	File : giet_config.h                                                    */
/*	Author : Alain Greiner                                                  */
/*	Date : 26/03/2013                                                       */
/****************************************************************************/
/* 	Define various configuration parameters for the GIET			        */
/****************************************************************************/

#ifndef _GIET_VM_CONFIG_H
#define _GIET_VM_CONFIG_H

/* hardware parameters */
#include "hard_config.h"

/* Debug parameters */

#define BOOT_DEBUG_MAPPING        0            /* map_info checking */
#define BOOT_DEBUG_PT             0            /* page tables initialisation */
#define BOOT_DEBUG_SCHED          0            /* schedulers initialisation */
#define BOOT_DEBUG_ELF            0            /* .elf files loading */

#define GIET_DEBUG_INIT           0            /* kernel initialisation */

#define GIET_DEBUG_FAT            0            /* fat access */ 
#define GIET_DEBUG_SIMPLE_LOCK    0            /* kernel simple lock access */
#define GIET_DEBUG_SPIN_LOCK      0            /* kernel spin lock access */
#define GIET_DEBUG_SQT_LOCK       0            /* kernel SQT lock access */
#define GIET_DEBUG_SIMPLE_BARRIER 0            /* kernel simple barrier access */
#define GIET_DEBUG_SQT_BARRIER    0            /* kernel SQT barrier access */
#define GIET_DEBUG_SYS_MALLOC     0            /* kernel malloc access */
#define GIET_DEBUG_SWITCH         0            /* context switchs  */
#define GIET_DEBUG_IRQS           0            /* interrupts */
#define GIET_DEBUG_IOC            0            /* IOC access: BDV, HBA, SDC, RDK */
#define GIET_DEBUG_TTY_DRIVER     0            /* TTY access */
#define GIET_DEBUG_DMA_DRIVER     0            /* DMA access */
#define GIET_DEBUG_NIC            0            /* NIC access */
#define GIET_DEBUG_FBF_CMA        0            /* FBF_CMA access */
#define GIET_DEBUG_COPROC         0            /* coprocessor access */
#define GIET_DEBUG_EXEC           0            /* kill/exec mechanism */

#define GIET_DEBUG_USER_MALLOC    0            /* malloc library */
#define GIET_DEBUG_USER_BARRIER   0            /* barrier library */
#define GIET_DEBUG_USER_MWMR      0            /* mwmr library */
#define GIET_DEBUG_USER_LOCK      0            /* user locks access */

#define CONFIG_SRL_VERBOSITY TRACE 

/* software parameters */

#define GIET_ELF_BUFFER_SIZE     0x80000       /* buffer for .elf files  */
#define GIET_IDLE_TASK_PERIOD    0x10000000    /* Idle Task message period */
#define GIET_OPEN_FILES_MAX      16            /* max simultaneously open files */
#define GIET_NB_VSPACE_MAX       16            /* max number of virtual spaces */
#define GIET_TICK_VALUE	         0x00010000    /* context switch period (cycles) */
#define GIET_USE_IOMMU           0             /* IOMMU activated when non zero */
#define GIET_NO_HARD_CC          0             /* No hard cache coherence */
#define GIET_NIC_MAC4            0x12345678    /* 32 LSB bits of the MAC address */
#define GIET_NIC_MAC2            0xBEBE        /* 16 MSB bits of the MAC address */
#define GIET_ISR_TYPE_MAX        32            /* max number of ISR types */
#define GIET_ISR_CHANNEL_MAX     8             /* max number of ISR channels */
#define GIET_SDC_PERIOD          2             /* number of system cycles in SDC period */
#define GIET_SR_INIT_VALUE       0x2000FF13    /* SR initial value (before eret) */

#endif

