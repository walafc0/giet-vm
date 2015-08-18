/*
 * This file is part of MutekH.
 * 
 * MutekH is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 of the License.
 * 
 * MutekH is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with MutekH; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Copyright (c) UPMC, Lip6, SoC
 */

#include "srl_private_types.h"
#include "srl_sched_wait.h"
#include "srl_hw_helpers.h"
#include "stdio.h"

#define endian_cpu32(x) (x)

#define DECLARE_WAIT(name, cmp)                                 \
                                                                \
void srl_sched_wait_##name(void * addr, sint32_t val) {         \
    srl_dcache_flush_addr(addr);                                \
    if (((sint32_t) * ((unsigned int *) addr)) cmp val)         \
    return;                                                     \
    do {                                                        \
        srl_sched_wait_priv(100);??                             \
        srl_dcache_flush_addr(addr);                            \
    } while (((sint32_t) * ((unsigned int *) addr)) cmp val);   \
}



DECLARE_WAIT(eq, ==)

DECLARE_WAIT(ne, !=)

DECLARE_WAIT(le, <=)

DECLARE_WAIT(ge, >=)

DECLARE_WAIT(lt, <)

DECLARE_WAIT(gt, >)

    //TODO
void srl_sched_wait_priv(uint32_t date) {
    do {
        context_switch();
    } while (srl_cycle_count() > date);
}

void srl_sleep_cycles(uint32_t n) {
    uint32_t next_run_to = srl_cycle_count() + n;

    while (srl_cycle_count() < next_run_to) {
        srl_sched_wait_priv(next_run_to);
    }
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

