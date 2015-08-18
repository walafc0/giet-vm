#ifndef SRL_MEMSPACE_H
#define SRL_MEMSPACE_H

#include "srl_public_types.h"

#include <memspace.h>

/**
 * @file
 * @module{SRL}
 * @short Memory resources
 */


/**
   The memspace abstract type.
 */


typedef giet_memspace_t * srl_memspace_t;

/**
   @this retrieves the size of a memspace

   @param memsp The memspace
   @return the size of the memspace
 */
#define SRL_MEMSPACE_SIZE(memsp) ((memsp)->size)
#define SRL_MEMSPACE_ADDR(memsp) ((memsp)->buffer)


#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

