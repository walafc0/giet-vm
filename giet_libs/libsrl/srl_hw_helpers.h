#ifndef SRL_HW_HELPERS_H
#define SRL_HW_HELPERS_H

/**
 * @file
 * @module{SRL}
 * @short Miscellaneous APIs
 */

#include "stdio.h"


/**
  Standard API call, expands to nothing for this implementation.
  */
#define srl_busy_cycles(n) do{}while(0)

//void useless(void *pointless,...){}
/**
  @this flushes the cache line containing the address.
  */
//TODO
#define srl_dcache_flush_addr 0

/*
   static inline cpu_dcache_invld(void *ptr){
   asm volatile (                                
   " cache %0, %1"                            
   : : "i" (0x11) , "R" (*(uint8_t*)(ptr))    
   : "memory"                                
   );                            
   }
   */

/**
  @this flushes a memory zone from cache.
  */
//TODO
//void dcache_flush(const void * addr, size_t size)
#define srl_dcache_flush_zone dcache_flush

/**
  @this waits for at least the given time (in cycles). The actual
  time spent in this call is not predictable.

  @param time Number of cycles to wait for
  */
void srl_sleep_cycles(unsigned int time);

/**
  @this returns the absolute timestamp counter from the
  initialization of the platform.

  @return Cycles from the initialization of the system
  */
static inline unsigned int srl_cycle_count() {
    return giet_proctime();
}


/**
  @this aborts the current execution. On most systems, @this will
  simply hang.
  */
static inline void srl_abort() {
    asm volatile ("break 0");
    while (1);
}


/**
 *
 */
static inline void srl_exit() {
    giet_exit();
}

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

