#ifndef SRL_PRIVATE_TYPES_H
#define SRL_PRIVATE_TYPES_H

/**
 * @file
 * @module{SRL}
 * @short Abstract types definitions
 */



/**
 * copy to the cache avoiding the optimisations done the compiler (volatile)
 */
#define cpu_mem_write_32(addr, data) *((volatile uint32_t*)(addr)) = data 

#endif
