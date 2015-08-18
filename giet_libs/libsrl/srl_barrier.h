/*
 * This file is part of DSX, development environment for static
 * SoC applications.
 * 
 * This file is distributed under the terms of the GNU General Public
 * License.
 * 
 *     Laboratoire d'informatique de Paris 6 / ASIM, France
 * 
 *  $Id$
 */

#ifndef SRL_BARRIER_H_
#define SRL_BARRIER_H_

/**
 * @file
 * @module{SRL}
 * @short Barrier operations
 */

#include "barrier.h"


typedef giet_barrier_t * srl_barrier_t;

#define srl_barrier_wait(bar) barrier_wait(bar)

#endif


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

