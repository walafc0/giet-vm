//////////////////////////////////////////////////////////////////////////////////
// File     : mwmr_channel.h         
// Date     : 01/04/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The mwmr_channel.c and mwmr_channel.h files are part of the GIET nano-kernel.
// This  middleware implements a user level Multi-Writers / Multi-Readers
// communication channel, that can be used by parallel multi-tasks applications
// respecting the TCG (Tasks and Communications Graph) formalism.
// 
// The mwmr_read() and mwmr_write() functions do not require a system call.
// The channel must have been allocated in a non cacheable segment,
// if the platform does not provide hardware cache coherence.
//
// An MWMR transaction transfer an integer number of items, and an item is
// an integer number of unsigned int (32 bits words).
// The max number of words that can be stored in a MWMR channel is defined by the
// "depth" parameter, and the "width" parameter define the minimal number of
// word contained in an atomic item. Therefore, the "depth" parameter must be
// a multiple of the "width" parameter.
//
// Both the mwmr_read() and mwmr_write() functions are blocking functions. 
// A queuing lock provides exclusive access to the MWMR channel.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _MWMR_CHANNEL_H_
#define _MWMR_CHANNEL_H_

#include "user_lock.h"

///////////////////////////////////////////////////////////////////////////////////
//  MWMR channel structure
///////////////////////////////////////////////////////////////////////////////////

typedef struct mwmr_channel_s 
{
    user_lock_t    lock;         // exclusive access lock
    unsigned int   sts;          // number of words available
    unsigned int   ptr;          // index of the first valid data word
    unsigned int   ptw;          // index of the first empty slot 
    unsigned int   depth;        // max number of words in the channel
    unsigned int   width;        // number of words in an item      
    unsigned int*  data;         // circular buffer base address
    unsigned int   padding[10];  // for 64 bytes alignment
} mwmr_channel_t;

//////////////////////////////////////////////////////////////////////////////
//  MWMR access functions
//////////////////////////////////////////////////////////////////////////////

void mwmr_init(  mwmr_channel_t* mwmr,
                 unsigned int*   buffer,    // data buffer base address
                 unsigned int    width,     // number of words per item
                 unsigned int    nitems );  // max number of items

void mwmr_read(  mwmr_channel_t* mwmr,
                 unsigned int*   buffer,
                 unsigned int    items );

void mwmr_write( mwmr_channel_t* mwmr,
                 unsigned int*   buffer,
                 unsigned int    items );

unsigned int nb_mwmr_read ( mwmr_channel_t * mwmr,
                            unsigned int * buffer,
                            unsigned int items );

unsigned int nb_mwmr_write( mwmr_channel_t * mwmr,
                            unsigned int * buffer,
                            unsigned int items );

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

