//////////////////////////////////////////////////////////////////////////////////
// File     : mwmr_channel.c         
// Date     : 01/04/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include "mwmr_channel.h"
#include "giet_config.h"
#include "stdio.h"
#include "user_lock.h"

//////////////////////////////////////
void mwmr_init( mwmr_channel_t*  mwmr,
                unsigned int*    buffer,     // buffer base address
                unsigned int     width,      // number of words per item
                unsigned int     nitems )    // max number of items
{

#if GIET_DEBUG_USER_MWMR
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] initialises fifo %x / "
                " buffer = %x / width = %d / nitems = %d\n",
                x, y, lpid, (unsigned int)mwmr, (unsigned int)buffer, width, nitems );
#endif

    mwmr->ptw   = 0;
    mwmr->ptr   = 0;
    mwmr->sts   = 0;
    mwmr->width = width;
    mwmr->depth = width * nitems;
    mwmr->data  = buffer;

    lock_init( &mwmr->lock );
}


///////////////////////////////////////////////////
unsigned int nb_mwmr_write( mwmr_channel_t * mwmr, 
                            unsigned int *   buffer, 
                            unsigned int     items)
{

#if GIET_DEBUG_USER_MWMR
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] enters nb_mwmr_write()"
                " : mwmr = %x / buffer = %x / items =  %d\n",
                x, y, lpid, (unsigned int)mwmr, (unsigned int)buffer, items );
#endif

    unsigned int n;
    unsigned int spaces; // number of empty slots (in words)
    unsigned int nwords; // requested transfer length (in words)
    unsigned int depth;  // channel depth (in words)
    unsigned int width;  // channel width (in words)
    unsigned int sts;    // channel sts
    unsigned int ptw;    // channel ptw

    if (items == 0) return 0;

    // get the lock
    lock_acquire( &mwmr->lock );

    // access fifo status
    depth  = mwmr->depth;
    width  = mwmr->width;
    sts    = mwmr->sts;
    ptw    = mwmr->ptw;
    spaces = depth - sts;
    nwords = width * items;

    if (spaces >= nwords) // transfer items, release lock and return 
    { 
        for (n = 0; n < nwords; n++) 
        {
            mwmr->data[ptw] = buffer[n];
            if ((ptw + 1) == depth)  ptw = 0; 
            else                     ptw = ptw + 1;
        }
        mwmr->sts = mwmr->sts + nwords;
        mwmr->ptw = ptw;

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] writes %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

        lock_release( &mwmr->lock );
        return items;
    }
    else if (spaces < width) // release lock and return 
    {
        lock_release( &mwmr->lock );
        return 0;
    }
    else // transfer as many items as possible, release lock and return 
    {
        nwords = (spaces / width) * width;    // integer number of items
        for (n = 0; n < nwords; n++) 
        {
            mwmr->data[ptw] = buffer[n];
            if ((ptw + 1) == depth) ptw = 0;
            else                    ptw = ptw + 1;
        }
        mwmr->sts = sts + nwords;
        mwmr->ptw = ptw;

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] writes %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

        lock_release( &mwmr->lock );
        return (nwords / width);
    }
} // end nb_mwmr_write()



//////////////////////////////////////////////////
unsigned int nb_mwmr_read( mwmr_channel_t * mwmr, 
                           unsigned int *   buffer,
                           unsigned int     items) 
{

#if GIET_DEBUG_USER_MWMR
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] enters nb_mwmr_read()"
                " : mwmr = %x / buffer = %x / items =  %d\n",
                x, y, lpid, (unsigned int)mwmr, (unsigned int)buffer, items );
#endif

    unsigned int n;
    unsigned int nwords; // requested transfer length (words)
    unsigned int depth;  // channel depth (words)
    unsigned int width;  // channel width (words)
    unsigned int sts;    // channel sts   (words)
    unsigned int ptr;    // channel ptr   (words)

    if (items == 0) return 0;

    // get the lock
    lock_acquire( &mwmr->lock );

    // access fifo status
    depth  = mwmr->depth;
    width  = mwmr->width;
    sts    = mwmr->sts;
    ptr    = mwmr->ptr;
    nwords = width * items;

    if (sts >= nwords) // transfer items, release lock and return 
    {
        for (n = 0; n < nwords; n++) 
        {
            buffer[n] = mwmr->data[ptr];
            if ((ptr + 1) == depth)  ptr = 0;
            else                     ptr = ptr + 1;
        }
        mwmr->sts = mwmr->sts - nwords;
        mwmr->ptr = ptr;

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] read %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

        lock_release( &mwmr->lock );
        return items;
    }
    else if (sts < width) // release lock and return 
    {

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] read nothing in fifo %x : sts = %d\n",
                x, y, lpid, (unsigned int)mwmr, mwmr->sts );
#endif

        lock_release( &mwmr->lock );
        return 0;
    }
    else // transfer as many items as possible, release lock and return 
    {
        nwords = (sts / width) * width; // integer number of items
        for (n = 0 ; n < nwords ; n++) 
        {
            buffer[n] = mwmr->data[ptr];
            if ((ptr + 1) == depth)  ptr = 0;
            else                     ptr = ptr + 1;
        }
        mwmr->sts = sts - nwords;
        mwmr->ptr = ptr;

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] read %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

        lock_release( &mwmr->lock );
        return (nwords / width);
    }
} // nb_mwmr_read()



////////////////////////////////////////
void mwmr_write( mwmr_channel_t * mwmr, 
                 unsigned int *   buffer, 
                 unsigned int     items ) 
{

#if GIET_DEBUG_USER_MWMR
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] enters mwmr_write()"
                " : mwmr = %x / buffer = %x / items =  %d\n",
                x, y, lpid, (unsigned int)mwmr, (unsigned int)buffer, items );
#endif

    unsigned int n;
    unsigned int spaces; // number of empty slots (in words)
    unsigned int nwords; // requested transfer length (in words)
    unsigned int depth;  // channel depth (in words)
    unsigned int width;  // channel width (in words)
    unsigned int sts;    // channel sts
    unsigned int ptw;    // channel ptw

    if (items == 0)  return;

    while (1) 
    {
        // get the lock
        lock_acquire( &mwmr->lock );

        // compute spaces and nwords
        depth = mwmr->depth;
        width = mwmr->width;
        sts  = mwmr->sts;
        ptw  = mwmr->ptw;
        spaces = depth - sts;
        nwords = width * items;

        if (spaces >= nwords) // write nwords, release lock and return
        {
            for (n = 0; n < nwords; n++) 
            {
                mwmr->data[ptw] = buffer[n];
                if ((ptw + 1) == depth)  ptw = 0; 
                else                     ptw = ptw + 1;
            }
            mwmr->ptw = ptw;
            mwmr->sts = sts + nwords;

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] writes %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

            lock_release( &mwmr->lock );
            return;
        }
        else if (spaces < width) // release lock and retry           
        {
            lock_release( &mwmr->lock );
        }
        else // write as many items as possible, release lock and retry
        {
            nwords = (spaces / width) * width;  // integer number of items
            for (n = 0; n < nwords; n++) 
            {
                mwmr->data[ptw] = buffer[n];
                if ((ptw + 1) == depth)  ptw = 0; 
                else                     ptw = ptw + 1;
            }
            mwmr->sts = sts + nwords;
            mwmr->ptw = ptw;
            buffer = buffer + nwords;
            items = items - (nwords/width);

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] writes %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

            lock_release( &mwmr->lock );
        }

        // we could deschedule before retry...
        // giet_context_switch();
    }
} // end mwmr_write()


//////////////////////////////////////
void mwmr_read( mwmr_channel_t * mwmr, 
                unsigned int *   buffer, 
                unsigned int     items) 
{

#if GIET_DEBUG_USER_MWMR
unsigned int    x;
unsigned int    y;
unsigned int    lpid;
giet_proc_xyp( &x, &y, &lpid );
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] enters mwmr_read()"
                " : mwmr = %x / buffer = %x / items =  %d\n",
                x, y, lpid, (unsigned int)mwmr, (unsigned int)buffer, items );
#endif

    unsigned int n;
    unsigned int nwords; // requested transfer length (in words)
    unsigned int depth;  // channel depth (in words)
    unsigned int width;  // channel width (in words)
    unsigned int sts;    // channel sts
    unsigned int ptr;    // channel ptr

    if (items == 0) return;

    while (1) 
    {
        // get the lock
        lock_acquire( &mwmr->lock );

        // compute nwords
        depth  = mwmr->depth;
        width  = mwmr->width;
        sts    = mwmr->sts;
        ptr    = mwmr->ptr;
        nwords = width * items;

        if (sts >= nwords) // read nwords, release lock and return
        {
            for (n = 0; n < nwords; n++) 
            {
                buffer[n] = mwmr->data[ptr];
                if ((ptr + 1) == depth)  ptr = 0;
                else                     ptr = ptr + 1;
            }
            mwmr->sts = mwmr->sts - nwords;
            mwmr->ptr = ptr;

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] read %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

            lock_release( &mwmr->lock );
            return;
        }
        else if (sts < width) // release lock and retry
        {
            lock_release( &mwmr->lock );
        }
        else // read as many items as possible, release lock and retry
        {   
            nwords = (sts / width) * width; // integer number of items
            for (n = 0; n < nwords; n++) 
            {
                buffer[n] = mwmr->data[ptr];
                if ((ptr + 1) == depth) ptr = 0;
                else                    ptr = ptr + 1;
            }
            mwmr->sts = sts - nwords;
            mwmr->ptr = ptr;
            buffer = buffer + nwords;
            items = items - (nwords/width);

#if GIET_DEBUG_USER_MWMR
giet_tty_printf("\n[MWMR DEBUG] Proc[%d,%d,%d] read %d words in fifo %x : sts = %d\n",
                x, y, lpid, nwords, (unsigned int)mwmr, mwmr->sts );
#endif

            lock_release( &mwmr->lock );
        }

        // we could deschedule before retry...
        // giet_context_switch();
    }
} // end mwmr_read() 


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

