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

#ifndef SRL_LOCK_H_
#define SRL_LOCK_H_

/**
 * @file
 * @module{SRL}
 * @short Lock operations
 */

#include "spin_lock.h"

typedef giet_lock_t * srl_lock_t;

/**
   @this takes a lock.

   @param lock The lock object
 */
#define srl_lock_lock(lock) lock_acquire(lock);

/**
   @this releases a lock.

   @param lock The lock object
 */
#define srl_lock_unlock( lock ) lock_release(lock);

/**
   @this tries to take a lock. @this returns whether the lock was
   actually taken.

   @param lock The lock object
   @return 0 if the lock was taken successfully
 */
#define srl_lock_try_lock( lock ) lock_try_acquire( lock );

#endif


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

