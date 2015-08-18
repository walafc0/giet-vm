#ifndef SRL_SCHED_WAIT_H
#define SRL_SCHED_WAIT_H

#include "srl_public_types.h"

/**
 * @file
 * @module{SRL}
 * @short Smart waiting tools
 */

#define DECLARE_WAIT(name, cmp)                                       \
/**                                                                           \
   @this makes the current task sleep until the value pointed at @tt addr     \
   asserts the following test:                                                \
                                                                              \
   @code                                                                      \
   (*addr cmp val)                                                            \
   @end code                                                                  \
                                                                              \
   @param addr The address to poll                                            \
   @param val The value to compare to                                         \
*/                                                                            \
                                                                              \
void srl_sched_wait_##name(void *addr, sint32_t val );

DECLARE_WAIT(eq, ==)
DECLARE_WAIT(ne, !=)
DECLARE_WAIT(le, <=)
DECLARE_WAIT(ge, >=)
DECLARE_WAIT(lt, <)
DECLARE_WAIT(gt, >)

#undef DECLARE_WAIT


void srl_sleep_cycles(unsigned int n);

#endif


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

