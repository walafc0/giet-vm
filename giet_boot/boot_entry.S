/*
 * file   : boot_entry.S
 * date   : 01/17/2014
 * author : Cesar Fuguet & Alain Greiner & Hao Liu
 *
 * This file contains the boot_entry() function that is the entry
 * point for the GIET_VM bootloader.
 * It supports a generic multi-clusters / multi-processors architecture,
 * containing at most 1024 processors.
 * - The number of clusters is defined by the (X_SIZE,Y_SIZE) parameters 
 *   in the hard_config.h file (up to 256 clusters).
 * - The number of processors per cluster is defined by the NB_PROCS_MAX 
 *   parameter in the hard_config.h file (up to 4 processors per cluster). 
 *
 * This assembly code is executed by all processors.
 * It initializes the stack pointer depending on the proc_id,
 * and using the SEG_BOOT_STACK_BASE and SEG_BOOT_STACK_SIZE 
 * parameters defined in the hard_config.h file, and jumps
 * to the boot_init() fuction defined in the boot.c file.
 *
 * - each processor P[x,y,0] uses a larger stack: 1,25 Kbytes. 
 * - Other processors use a smaller stack:        0,25 Kbytes.
 *     => the SEG_BOOT_STACK_SIZE cannot be smaller than 
 *        256 * (1024 + 256) + (1024 - 256) * 256 = 512 Kbytes
 */

    #include "mips32_registers.h"
    #include "hard_config.h"

    .section .text,"ax",@progbits

    .globl  boot_entry 
    .ent    boot_entry

    .align  2
    .set noreorder

boot_entry:

    /* The (x,y,lpid) values are obtained from the processor CP0 register,        */
    /* where  proc_id == (((x<<Y_WIDTH) + y)<<P_WIDTH) + lpid (fixed format)      */ 
    /* The continuous cluster index is computed as cluster_id = (x * Y_SIZE) + y  */

    mfc0   k0,      CP0_PROCID
    andi   k0,      k0,     0xFFF             /* k0 <= proc_id                    */
    andi   t1,      k0,     ((1<<P_WIDTH)-1)  /* t1 <= lpid                       */
    srl    t2,      k0,     P_WIDTH           /* t2 <= cluster_xy                 */
    srl    t3,      t2,     Y_WIDTH           /* t3 <= x coordinate               */
    andi   t4,      t2,     ((1<<Y_WIDTH)-1)  /* t4 <= y coordinate               */
    la     t6,      Y_SIZE                    /* t6 <= Y_SIZE                     */
    multu  t3,      t6
    mflo   t5                                 /* t5 <= x * Y_SIZE                 */
    addu   t5,      t5,     t4                /* t5 <= cluster_id                 */

    /* All processors initializes stack pointer, depending on x,y,lpid            */
    /* Processors P[x,y,0] : stack size = 1,25 Kbytes                             */
    /* Other processors    : stack size = 0,25 Kbytes                             */
    /* In each cluster, the total stack size is NB_PROCS_MAX-1)*0x100 + 0x500     */

    li     t6,      (NB_PROCS_MAX-1) * 0x100 + 0x500  /* t6 <= cluster_size       */
    multu  t6,      t5
    mflo   t7                                 /* t7 <= cluster_size * cluster_id  */
    la     k0,      SEG_BOOT_STACK_BASE
    addu   k0,      k0,     t7                /* k0 <= stack base in cluster      */
    li     k1,      0x500                     /* k1 <= 1,25 Kbytes                */
    addu   sp,      k0,     k1                /* P[x,y,0] stack top               */
    li     k1,      0x100                     /* k1 <= 0,25 bytes                 */
    multu  k1,      t1              
    mflo   k0                                 /* k0 <= 256 * lpid                 */
    addu   sp,      sp,     k0                /* P[x,y,lpid] stack top            */ 

    /* All processors jump to the boot_init function                              */

    la	   k0,     boot_init
    jr     k0
    nop

    .end boot_entry

    .set reorder

/*
 * vim: tabstop=4 : shiftwidth=4 : expandtab
 */
