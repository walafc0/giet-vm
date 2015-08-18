/******************************************************************************
* This function receives two arguments that are the current task context 
* (virtual) addresses and the next task context (virtual) address.
*
* This function should be called in a critical section
*
* TODO (AG) Il semble possible de limiter le nombre de registres à sauver:
* - s0 à s8 ($16 à $23 + $30)
* - sp ($29)
* - ra ($31)
* - hi et lo
* - sr
* - epc
* - ptpr
******************************************************************************/

    .globl  _task_switch
    .func   _task_switch
    .type   _task_switch, %function

_task_switch:

    /* save _current task context */
    add     $27,    $4,     $0  /* $27 <= &context[curr_task_id] */

    .set noat
    sw      $1,     1*4($27)    /* ctx[1] <= $1 */
    .set at
    sw      $2,     2*4($27)    /* ctx[2] <= $2 */
    sw      $3,     3*4($27)    /* ctx[3] <= $3 */
    sw      $4,     4*4($27)    /* ctx[4] <= $4 */
    sw      $5,     5*4($27)    /* ctx[5] <= $5 */
    sw      $6,     6*4($27)    /* ctx[6] <= $6 */
    sw      $7,     7*4($27)    /* ctx[7] <= $7 */
    sw      $8,     8*4($27)    /* ctx[8] <= $8 */
    sw      $9,     9*4($27)    /* ctx[9] <= $9 */
    sw      $10,    10*4($27)   /* ctx[10] <= $10 */
    sw      $11,    11*4($27)   /* ctx[11] <= $11 */
    sw      $12,    12*4($27)   /* ctx[12] <= $12 */
    sw      $13,    13*4($27)   /* ctx[13] <= $13 */
    sw      $14,    14*4($27)   /* ctx[14] <= $14 */
    sw      $15,    15*4($27)   /* ctx[15] <= $15 */
    sw      $16,    16*4($27)   /* ctx[16] <= $16 */
    sw      $17,    17*4($27)   /* ctx[17] <= $17 */
    sw      $18,    18*4($27)   /* ctx[18] <= $18 */
    sw      $19,    19*4($27)   /* ctx[19] <= $19 */
    sw      $20,    20*4($27)   /* ctx[20] <= $20 */
    sw      $21,    21*4($27)   /* ctx[21] <= $21 */
    sw      $22,    22*4($27)   /* ctx[22] <= $22 */
    sw      $23,    23*4($27)   /* ctx[23] <= $23 */
    sw      $24,    24*4($27)   /* ctx[24] <= $24 */
    sw      $25,    25*4($27)   /* ctx[25] <= $25 */
    mflo    $26
    sw      $26,    26*4($27)   /* ctx[26] <= LO  */
    mfhi    $26
    sw      $26,    27*4($27)   /* ctx[27] <= H1  */
    sw      $28,    28*4($27)   /* ctx[28] <= $28 */
    sw      $29,    29*4($27)   /* ctx[29] <= $29 */
    sw      $30,    30*4($27)   /* ctx[30] <= $30 */
    sw      $31,    31*4($27)   /* ctx[31] <= $31 */
    mfc0    $26,    $14
    sw      $26,    32*4($27)   /* ctx[32] <= EPC */
    mfc0    $26,    $13
    sw      $26,    33*4($27)   /* ctx[33] <= CR  */
    mfc0    $26,    $12
    sw      $26,    34*4($27)   /* ctx[34] <= SR  */
    mfc0    $26,    $8
    sw      $26,    35*4($27)   /* ctx[34] <= BVAR */
    mfc2    $26,    $0
    sw      $26,    39*4($27)   /* ctx[35] <= PTPR */

    /* restore next task context */
    add     $27,    $5,     $0  /* $27<= &context[next_task_id] */

    .set noat
    lw      $1,     1*4($27)    /* restore $1 */
    .set at
    lw      $2,     2*4($27)    /* restore $2 */
    lw      $3,     3*4($27)    /* restore $3 */
    lw      $4,     4*4($27)    /* restore $4 */
    lw      $5,     5*4($27)    /* restore $5 */
    lw      $6,     6*4($27)    /* restore $6 */
    lw      $7,     7*4($27)    /* restore $7 */
    lw      $8,     8*4($27)    /* restore $8 */
    lw      $9,     9*4($27)    /* restore $9 */
    lw      $10,    10*4($27)   /* restore $10 */
    lw      $11,    11*4($27)   /* restore $11 */
    lw      $12,    12*4($27)   /* restore $12 */
    lw      $13,    13*4($27)   /* restore $13 */
    lw      $14,    14*4($27)   /* restore $14 */
    lw      $15,    15*4($27)   /* restore $15 */
    lw      $16,    16*4($27)   /* restore $16 */
    lw      $17,    17*4($27)   /* restore $17 */
    lw      $18,    18*4($27)   /* restore $18 */
    lw      $19,    19*4($27)   /* restore $19 */
    lw      $20,    20*4($27)   /* restore $20 */
    lw      $21,    21*4($27)   /* restore $21 */
    lw      $22,    22*4($27)   /* restore $22 */
    lw      $23,    23*4($27)   /* restore $23 */
    lw      $24,    24*4($27)   /* restore $24 */
    lw      $25,    25*4($27)   /* restore $25 */
    lw      $26,    26*4($27)
    mtlo    $26                 /* restore LO */
    lw      $26,    27*4($27)
    mthi    $26                 /* restore HI */
    lw      $28,    28*4($27)   /* restore $28 */
    lw      $29,    29*4($27)   /* restore $29 */
    lw      $30,    30*4($27)   /* restore $30 */
    lw      $31,    31*4($27)   /* restore $31 */
    lw      $26,    32*4($27)
    mtc0    $26,    $14         /* restore EPC */
    lw      $26,    33*4($27)
    mtc0    $26,    $13         /* restore CR */
    lw      $26,    34*4($27)
    mtc0    $26,    $12         /* restore SR */
    lw      $26,    35*4($27)
    mtc0    $26,    $8          /* restore BVAR */
    lw      $26,    39*4($27)
    mtc2    $26,    $0          /* restore PTPR */

    /* returns to caller */
    jr      $31 

    .endfunc
    .size _task_switch, .-_task_switch

