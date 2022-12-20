#include <stdint.h>

#include "stm32f4xx.h"  // Pulls in core_cm4.h with DWT defs

#include "cstone/cycle_counter_cortex.h"


void cycle_counter_init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  // NOTE: Needed for CM7  DWT->LAR = 0xC5ACCE55; 
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


uint32_t cycle_count(void) {
  return DWT->CYCCNT;
}
