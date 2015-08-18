/////////////////////////////////////////////////////////////////////////////////
// File     : ctx_handler.h
// Date     : 01/04/2012
// Authors  : alain greiner & joel porquet
// Copyright (c) UPMC-LIP6
/////////////////////////////////////////////////////////////////////////////////
// The ctx_handler.h and ctx_handler.c files are part of the GIET-VM nano-kernel.
// This code is used to support context switch when several tasks are executing
// in time multiplexing on a single processor.
// The tasks are statically allocated to a processor in the boot phase, and
// there is one private scheduler per processor. Each sheduler occupies 8K bytes, 
// and contains up to 14 task contexts (task_id is from 0 to 13).
// The task context [13] is reserved for the "idle" task that does nothing, and
// is launched by the scheduler when there is no other runable task.
/////////////////////////////////////////////////////////////////////////////////
// A task context is an array of 64 uint32 words => 256 bytes. 
// It contains copies of processor registers (when the task is preempted)
// and some general informations associated to a task, such as the private
// peripheral channels allocated to the task, the vspace index, the various
// task index (local / global / application), and the runnable status.
/////////////////////////////////////////////////////////////////////////////////
// ctx[0] <- ***   |ctx[8] <- $8     |ctx[16]<- $16    |ctx[24]<- $24
// ctx[1] <- $1    |ctx[9] <- $9     |ctx[17]<- $17    |ctx[25]<- $25
// ctx[2] <- $2    |ctx[10]<- $10    |ctx[18]<- $18    |ctx[26]<- LO
// ctx[3] <- $3    |ctx[11]<- $11    |ctx[19]<- $19    |ctx[27]<- HI
// ctx[4] <- $4    |ctx[12]<- $12    |ctx[20]<- $20    |ctx[28]<- $28
// ctx[5] <- $5    |ctx[13]<- $13    |ctx[21]<- $21    |ctx[29]<- SP
// ctx[6] <- $6    |ctx[14]<- $14    |ctx[22]<- $22    |ctx[30]<- $30
// ctx[7] <- $7    |ctx[15]<- $15    |ctx[23]<- $23    |ctx[31]<- RA
//
// ctx[32]<- EPC   |ctx[40]<- TTY    |ctx[48]<- TRDID  |ctx[56]<- ***
// ctx[33]<- CR    |ctx[41]<- CMA_FB |ctx[49]<- GTID   |ctx[57]<- ***
// ctx[34]<- SR    |ctx[42]<- CMA_RX |ctx[50]<- NORUN  |ctx[58]<- ***
// ctx[35]<- BVAR  |ctx[43]<- CMA_TX |ctx[51]<- COPROC |ctx[59]<- ***
// ctx[36]<- PTAB  |ctx[44]<- NIC_RX |ctx[52]<- ENTRY  |ctx[60]<- ***
// ctx[37]<- LTID  |ctx[45]<- NIC_TX |ctx[53]<- SIG    |ctx[61]<- ***
// ctx[38]<- VSID  |ctx[46]<- TIM    |ctx[54]<- ***    |ctx[62]<- ***
// ctx[39]<- PTPR  |ctx[47]<- HBA    |ctx[55]<- ***    |ctx[63]<- ***
/////////////////////////////////////////////////////////////////////////////////

#ifndef _CTX_HANDLER_H
#define _CTX_HANDLER_H

#include <giet_config.h>

/////////////////////////////////////////////////////////////////////////////////
//    Definition of the task context slots indexes
/////////////////////////////////////////////////////////////////////////////////

#define CTX_SP_ID        29    // Stack Pointer
#define CTX_RA_ID        31    // Return Address

#define CTX_EPC_ID       32    // Exception Program Counter (CP0)
#define CTX_CR_ID        33    // Cause Register (CP0)
#define CTX_SR_ID        34    // Status Register (CP0)
#define CTX_BVAR_ID      35	   // Bad Virtual Address Register (CP0)
#define CTX_PTAB_ID      36    // Page Table Virtual address
#define CTX_LTID_ID      37    // Local  Task Index (in scheduler)
#define CTX_VSID_ID      38    // Vspace Index     
#define CTX_PTPR_ID      39    // Page Table Pointer Register (PADDR>>13)

#define CTX_TTY_ID       40    // private TTY channel index  
#define CTX_CMA_FB_ID    41    // private CMA channel index for FBF write
#define CTX_CMA_RX_ID    42    // private CMA channel index for NIC_TX
#define CTX_CMA_TX_ID    43    // private CMA channel index for NIC_RX
#define CTX_NIC_RX_ID    44    // private NIC channel index RX transfer
#define CTX_NIC_TX_ID    45    // private NIC channel index TX transfer
#define CTX_TIM_ID       46    // ptivate TIM channel index
#define CTX_HBA_ID       47    // private HBA channel index

#define CTX_TRDID_ID     48    // Thread Task Index in vspace
#define CTX_GTID_ID      49    // Global Task Index in all system
#define CTX_NORUN_ID     50    // bit-vector : task runable if all zero
#define CTX_COPROC_ID    51    // cluster_xy : coprocessor coordinates
#define CTX_ENTRY_ID     52    // Virtual address of task entry point
#define CTX_SIG_ID       53    // bit-vector : pending signals for task

/////////////////////////////////////////////////////////////////////////////////
//    Definition of the NORUN bit-vector masks
/////////////////////////////////////////////////////////////////////////////////

#define NORUN_MASK_TASK       0x00000001   // Task not active  
#define NORUN_MASK_IOC        0x00000002   // Task blocked on IOC transfer
#define NORUN_MASK_COPROC     0x00000004   // Task blocked on COPROC transfer

/////////////////////////////////////////////////////////////////////////////////
//    Definition of the SIG bit-vector masks
/////////////////////////////////////////////////////////////////////////////////

#define SIG_MASK_KILL         0x00000001   // Task will be killed at next tick
#define SIG_MASK_EXEC         0x00000002   // Task will be executed at next tick

/////////////////////////////////////////////////////////////////////////////////
//    Definition of the scheduler structure
/////////////////////////////////////////////////////////////////////////////////

typedef struct static_scheduler_s 
{
    unsigned int context[14][64];      // at most 14 task (including idle_task)
    unsigned int tasks;                // actual number of tasks
    unsigned int current;              // current task index
    unsigned int hwi_vector[32];       // hardware interrupt vector
    unsigned int pti_vector[32];       // timer    interrupt vector
    unsigned int wti_vector[32];       // software interrupt vector
    unsigned int reserved[30];         // padding to 4 Kbytes
    unsigned int idle_stack[1024];     // private stack for idle stack (4Kbytes)
} static_scheduler_t;

#define IDLE_TASK_INDEX        13


/////////////////////////////////////////////////////////////////////////////////
//               Extern Functions
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// This function performs a context switch between the running task
// and  another task, using a round-robin sheduling policy between all
// tasks allocated to a given processor (static allocation).
// It selects the next runable task to resume execution. 
// If the only runable task is the current task, return without context switch.
// If there is no runable task, the scheduler switch to the default "idle" task.
// The return address contained in $31 is saved in the current task context
// (in the ctx[31] slot), and the function actually returns to the address
// contained in the ctx[31] slot of the next task context.
/////////////////////////////////////////////////////////////////////////////////
extern void _ctx_switch();

/////////////////////////////////////////////////////////////////////////////////
// The address of this function is used to initialise the return address
// in the "idle" task context.
/////////////////////////////////////////////////////////////////////////////////
extern void _ctx_eret();

/////////////////////////////////////////////////////////////////////////////////
// This function is executed task when no other task can be executed.
/////////////////////////////////////////////////////////////////////////////////
extern void _idle_task();

/////////////////////////////////////////////////////////////////////////////////
// This function displays the context of a task identified by the processor
// coordinates (x,y,p), and by the local task index ltid.
// The string argument can be used for debug.
/////////////////////////////////////////////////////////////////////////////////
extern void _ctx_display( unsigned int x,
                          unsigned int y,
                          unsigned int p,
                          unsigned int ltid,
                          char*        string );

#endif
