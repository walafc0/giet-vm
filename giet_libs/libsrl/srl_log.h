/*
 * This file is part of DSX, development environment for static
 * SoC applications.
 * 
 * This file is distributed under the terms of the GNU General Public
 * License.
 * 
 * Copyright (c) 2006, Nicolas Pouillon, <nipo@ssji.net>
 *     Laboratoire d'informatique de Paris 6 / ASIM, France
 * 
 *  $Id$
 */

#ifndef SRL_LOG_H_
#define SRL_LOG_H_

/**
 * @file
 * @module{SRL}
 * @short Debug messages
 */

#include "stdio.h"
#include "giet_config.h"
#include "srl_hw_helpers.h"

/** @internal */
enum __srl_verbosity {
    VERB_NONE,
    VERB_TRACE,
    VERB_DEBUG,
    VERB_MAX,
};


void _srl_log(const char *);
void _srl_log_printf(const char *, ...);

#define GET_VERB_(x,y) x##y
#define GET_VERB(x) GET_VERB_(VERB_,x)

/**
   @this prints a message if the current verbosity is sufficient.

   @param l Minimal verbosity of the message
   @param c Message to print
 */
#define srl_log( l, c ) do {										   \
		if (GET_VERB(l) <= GET_VERB(CONFIG_SRL_VERBOSITY)) {		   \
            giet_tty_printf(c);                                             \
		}															   \
	} while (0)

/**
   @this prints a message if the current verbosity is sufficient.

   @param l Minimal verbosity of the message
   @param c Message to print, with a printf-like syntax
 */
#define srl_log_printf( l, ... ) do {                                   \
		if (GET_VERB(l) <= GET_VERB(CONFIG_SRL_VERBOSITY)) {		    \
            giet_tty_printf(__VA_ARGS__);                                    \
		}															    \
	} while (0)

/**
   @this is the same as @ref #assert.
    TODO change the location?
 */
#define srl_assert(expr)												\
    do {																\
        if ( ! (expr) ) {												\
            srl_log_printf(NONE, "assertion (%s) failed on %s:%d !\n",  \
						   #expr, __FILE__, __LINE__ );					\
            srl_abort();													\
        }																\
    } while(0)

#endif
