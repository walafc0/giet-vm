#ifndef SRL_MWMR_H_
#define SRL_MWMR_H_

#include "mwmr_channel.h"

typedef  mwmr_channel_t * srl_mwmr_t;

#define srl_mwmr_write(a, b, c) mwmr_write(a, (unsigned int *) b, (unsigned int) c)
#define srl_mwmr_read(a, b, c) mwmr_read(a, (unsigned int *) b, (unsigned int) c)
#define srl_mwmr_try_write(a, b, c) nb_mwmr_write(a, (unsigned int *) b, (unsigned int) c)
#define srl_mwmr_try_read(a, b, c) nb_mwmr_read(a, (unsigned int *) b, (unsigned int) c)


#endif //fin de SRL_MWMR_H_

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

