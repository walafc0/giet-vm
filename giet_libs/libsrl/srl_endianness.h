#ifndef SRL_ENDIANNESS_H_
#define SRL_ENDIANNESS_H_

#include "srl_public_types.h"

/** @this reads a big endian 16 bits value */
#  define endian_le16(x)    (x)
/** @this reads a big endian 32 bits value */
#  define endian_le32(x)    (x)
/** @this reads a big endian 64 bits value */
//#  define endian_le64(x)    (x)
/** @this reads a little endian 16 bits value */
#  define endian_be16(x)    endian_swap16(x)
/** @this reads a little endian 32 bits value */
#  define endian_be32(x)    endian_swap32(x)
/** @this reads a little endian 64 bits value */
//#  define endian_be64(x)    endian_swap64(x)

/** @internal */
static inline uint16_t endian_swap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}


/** @internal */
static inline uint32_t endian_swap32(uint32_t x) {
    return (((x >> 24) & 0x000000ff) |
            ((x >> 8 ) & 0x0000ff00) |
            ((x << 8 ) & 0x00ff0000) |
            ((x << 24) & 0xff000000));
}


/** @internal *//*
                   static inline uint64_t __endian_swap64(uint64_t x)
                   {
                   return (((uint64_t)endian_swap32(x      ) << 32) |
                   ((uint64_t)endian_swap32(x >> 32)      ));
                   }*/

static inline uint32_t srl_uint32_le_to_machine(uint32_t x) {
    return endian_le32(x);
}


static inline uint32_t srl_uint32_machine_to_le(uint32_t x) {
    return endian_le32(x);
}


static inline uint32_t srl_uint32_be_to_machine(uint32_t x) {
    return endian_be32(x);
}


static inline uint32_t srl_uint32_machine_to_be(uint32_t x) {
    return endian_be32(x);
}


static inline uint16_t srl_uint16_le_to_machine(uint16_t x) {
    return endian_le16(x);
}


static inline uint16_t srl_uint16_machine_to_le(uint16_t x) {
    return endian_le16(x);
}


static inline uint16_t srl_uint16_be_to_machine(uint16_t x) {
    return endian_be16(x);
}


static inline uint16_t srl_uint16_machine_to_be(uint16_t x) {
    return endian_be16(x);
}


#endif


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

