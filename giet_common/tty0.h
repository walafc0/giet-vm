///////////////////////////////////////////////////////////////////////////////////
// File     : tty0.h
// Date     : 02/12/2014
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
// The tty0.c and tty0.h files are part of the GIET-VM nano-kernel.
// They define the functions that can be used by both the boot code 
// and the kernel code to access the TTY0 kernel terminal.
///////////////////////////////////////////////////////////////////////////////////

#ifndef GIET_TTY0_H
#define GIET_TTY0_H

///////////////////////////////////////////////////////////////////////////////////
//       Access functions to kernel terminal TTY0
///////////////////////////////////////////////////////////////////////////////////

extern void         _puts( char*  string );

extern void         _putx( unsigned int val );

extern void         _putl( unsigned long long val );

extern void         _putd( unsigned int val ); 

extern void         _getc( char* byte );       

extern void         _nolock_printf( char* format, ... );

extern void         _printf( char* format, ... );

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

