//////////////////////////////////////////////////////////////////////////////////
// File     : user_lock.c         
// Date     : 01/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The user_lock.c and user_lock.h files are part of the GIET-VM nano-kernel.
///////////////////////////////////////////////////////////////////////////////////

#include "user_lock.h"
#include "giet_config.h"
#include "stdio.h"

//////////////////////////////////////////////////////////////////////////////////
// This function uses LL/SC to make an atomic increment. 
//////////////////////////////////////////////////////////////////////////////////
unsigned int atomic_increment( unsigned int*  ptr,
                               unsigned int   increment )
{
    unsigned int value;

    asm volatile (
        "1234:                         \n"
        "move $10,   %1                \n"   /* $10 <= ptr               */
        "move $11,   %2                \n"   /* $11 <= increment         */
        "ll   $12,   0($10)            \n"   /* $12 <= *ptr              */
        "addu $13,   $11,    $12       \n"   /* $13 <= *ptr + increment  */
        "sc   $13,   0($10)            \n"   /* M[ptr] <= new            */ 
        "beqz $13,   1234b             \n"   /* retry if failure         */
        "move %0,    $12               \n"   /* value <= *ptr if success */
        : "=r" (value) 
        : "r" (ptr), "r" (increment)
        : "$10", "$11", "$12", "$13", "memory" );

    return value;
}

///////////////////////////////////////////////////////////////////////////////////
// This blocking function returns only when the lock has been taken.
///////////////////////////////////////////////////////////////////////////////////
void lock_acquire( user_lock_t* lock ) 
{
    // get next free slot index from user_lock
    unsigned int ticket = atomic_increment( &lock->free, 1 );

#if GIET_DEBUG_USER_LOCK
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[USER_LOCK DEBUG] P[%d,%d,%d] get ticket = %d"
                " for lock %x at cycle %d (current = %d / free = %d)\n",
                x, y, lpid, ticket, 
                (unsigned int)lock, giet_proctime(), lock->current, lock->free );
#endif

    // poll the current slot index
    asm volatile("1793:                       \n"
                 "lw   $10,  0(%0)            \n"
                 "move $11,  %1               \n"
                 "bne  $10,  $11,  1793b      \n"
                 :
                 : "r"(lock), "r"(ticket)
                 : "$10", "$11" );
               
#if GIET_DEBUG_USER_LOCK
giet_tty_printf("\n[USER_LOCK DEBUG] P[%d,%d,%d] get lock %x"
                " at cycle %d (current = %d / free = %d)\n",
                x, y, lpid, (unsigned int)lock, 
                giet_proctime(), lock->current, lock->free );
#endif

}

//////////////////////////////////////////////////////////////////////////////
// This function releases the lock.
//////////////////////////////////////////////////////////////////////////////
void lock_release( user_lock_t* lock ) 
{
    asm volatile( "sync" );

    lock->current = lock->current + 1;

#if GIET_DEBUG_USER_LOCK
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[USER_LOCK DEBUG] P[%d,%d,%d] release lock %x"
                " at cycle %d (current = %d / free = %d)\n",
                x, y, lpid, (unsigned int)lock, 
                giet_proctime(), lock->current, lock->free );
#endif

}

//////////////////////////////////////////////////////////////////////////////
// This function initializes the lock.
//////////////////////////////////////////////////////////////////////////////
void lock_init( user_lock_t* lock )
{
    lock->current = 0;
    lock->free    = 0;

#if GIET_DEBUG_USER_LOCK
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[USER_LOCK DEBUG] P[%d,%d,%d] init lock %x"
                " at cycle %d (current = %d / free = %d)\n",
                x, y, lpid, (unsigned int)lock,
                giet_proctime(), lock->current, lock->free );
#endif

}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

