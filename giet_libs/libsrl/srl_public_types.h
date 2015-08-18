#ifndef SRL_PUBLIC_TYPES_H
#define SRL_PUBLIC_TYPES_H

/**
 * @file
 * @module{SRL}
 * @short Abstract types definitions
 */

typedef unsigned long       uint_t;
typedef signed long         sint_t;

typedef unsigned char       uint8_t;
typedef signed char         sint8_t;
typedef signed short int    int8_t;

typedef unsigned short int  uint16_t;
typedef signed short int    sint16_t;
typedef signed short int    int16_t;

typedef unsigned int        uint32_t;
typedef signed int          sint32_t;
typedef signed int          int32_t;

typedef sint32_t            error_t;
typedef uint_t              bool_t;

typedef void *srl_buffer_t;


#ifndef NULL
#define NULL                (void*)0
#endif

/** Get integer minimum value */
#define __MIN(a, b) ({ const typeof(a) __a = (a); const typeof(b) __b = (b); __b < __a ? __b : __a; })
    
/** Get integer maximum value */
#define __MAX(a, b) ({ const typeof(a) __a = (a); const typeof(b) __b = (b); __b > __a ? __b : __a; })


#endif
