#include <stdint.h>
#include <stdbool.h>

#include "build_config.h" // Get build-specific platform settings
#include "cstone/platform.h"

#if defined PLATFORM_HOSTED
#  include <unistd.h>
#  include <time.h>
#elif defined PLATFORM_STM32
#  define STM32_HAL_LEGACY  // Inhibit legacy header
#  if defined PLATFORM_STM32F1
#    include "stm32f1xx_hal.h"
#  else
#    include "stm32f4xx_hal.h"
#  endif
#endif

#include "FreeRTOS.h"
#include "cstone/rtos.h"
#include "cstone/timing.h"



#if defined PLATFORM_HOSTED
unsigned long millis(void) {
  unsigned long msec;
  struct timespec ts;

#  if defined _POSIX_VERSION
  clock_gettime(CLOCK_MONOTONIC, &ts);
#  else
  timespec_get(&ts, TIME_UTC);
#  endif

  msec = ((unsigned long)ts.tv_sec * 1000ul) + ((unsigned long)((ts.tv_nsec + 500000ul) / 1000000ul));
  return msec;
}


unsigned long micros(void) {
  unsigned long usec;
  struct timespec ts;

#  if defined _POSIX_VERSION
  clock_gettime(CLOCK_MONOTONIC, &ts);
#  else
  timespec_get(&ts, TIME_UTC);
#  endif

  usec = ((unsigned long)ts.tv_sec * 1000000ul) + ((unsigned long)((ts.tv_nsec + 500) / 1000ul));
  return usec;
}


void delay_millis(uint32_t msec) {
  usleep(msec * 1000);
}




#else // PLATFORM_STM32
unsigned long millis(void) {
  return HAL_GetTick();
}

unsigned long micros(void) {
  return (unsigned long)perf_timer_count() * (1000000ul / PERF_CLOCK_HZ);
}


// Blocking delay that does not depend on interrupts
// This depends on the free running performance timer initialized by perf_timer_init()
void delay_millis(uint32_t msec) {
  uint32_t ticks = msec * PERF_CLOCK_HZ / 1000ul;
  uint32_t start = perf_timer_count();
  uint32_t now = start;

  while(now - start < ticks) {
    now = perf_timer_count();
  }
}


#endif

