///////////////////////////////////////////////////////////////////////////////////
// File     : exc_handler.h
// Date     : 01/04/2012
// Author   : alain greiner and joel porquet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The exc_handler.c and exc_handler.h files are part of the GIET nano-kernel.
// They define the exception handler code.
///////////////////////////////////////////////////////////////////////////////////


#ifndef _EXCP_HANDLER_H
#define _EXCP_HANDLER_H

///////////////////////////////////////////////////////////////////////////////////
// Exception Vector Table (indexed by cause register XCODE field)
///////////////////////////////////////////////////////////////////////////////////

typedef void (*_exc_func_t)(void);

extern const _exc_func_t _cause_vector[16];

#endif

