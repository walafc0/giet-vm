//////////////////////////////////////////////////////////////////////////////////
// File     : user_lock.h         
// Date     : 01/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The file_lock.c and file_lock.h files are part of the GIET-VM nano-kernel.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _GIET_FILE_LOCK_H_
#define _GIET_FILE_LOCK_H_

///////////////////////////////////////////////////////////////////////////////////
//  lock structure
///////////////////////////////////////////////////////////////////////////////////

typedef struct user_lock_s 
{
    unsigned int current;        // current slot index
    unsigned int free;           // next free slot index
    unsigned int padding[14];    // for 64 bytes alignment
} user_lock_t;

///////////////////////////////////////////////////////////////////////////////////
//  access functions
///////////////////////////////////////////////////////////////////////////////////

extern unsigned int atomic_increment( unsigned int* ptr,
                                      unsigned int  increment );

extern void lock_acquire( user_lock_t * lock );

extern void lock_release( user_lock_t * lock );

extern void lock_init( user_lock_t * lock );

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

