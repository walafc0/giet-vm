
#include "srl_private_types.h"


////////////////////////////////////////////////////////////////////////////////////////
//  mempcy()
// GCC requires this function. Taken from MutekH.
////////////////////////////////////////////////////////////////////////////////////////
void * memcpy(void *_dst, const void * _src, unsigned int size) {
    unsigned int * dst = _dst;
    const unsigned int * src = _src;
    if (!((unsigned int) dst & 3) && !((unsigned int) src & 3) )
        while (size > 3) {
            *dst++ = *src++;
            size -= 4;
        }

    unsigned char *cdst = (unsigned char*)dst;
    unsigned char *csrc = (unsigned char*)src;

    while (size--) {
        *cdst++ = *csrc++;
    }
    return _dst;
}


////////////////////////////////////////////////////////////////////////////////////////
//  mempcy()
// GCC requires this function. Taken from MutekH.
////////////////////////////////////////////////////////////////////////////////////////
inline void * memset(void * dst, int s, size_t count) {
    char * a = (char *) dst;
    while (count--){
        *a++ = (char)s;
    }
    return dst;
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

