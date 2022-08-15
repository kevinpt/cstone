#include <stdint.h>
#include <stdbool.h>

#include "cstone/platform.h"
#include "lib_cfg/cstone_cfg_stm32.h"

#include "stm32f4xx_ll_tim.h"

#include "FreeRTOS.h"
#include "timers.h"

#include "cstone/rtos.h"


uint32_t perf_timer_count(void) {
#ifdef PLATFORM_EMBEDDED
  return LL_TIM_GetCounter(PERF_TIMER);
#else
  return micros() / (1000000ul / PERF_CLOCK_HZ);
#endif
}


// Initialize free running timer with 100us resolution for run time stats
void perf_timer_init(void) {
#ifdef PLATFORM_EMBEDDED
  TIM_HandleTypeDef perf_timer = {0};

  PERF_TIMER_CLK_ENABLE();

  // Base clock is HCLK/2
  uint16_t prescaler = (uint32_t) ((SystemCoreClock / 2) / PERF_CLOCK_HZ) - 1;

  perf_timer.Instance               = PERF_TIMER;
  perf_timer.Init.Period            = 0xFFFFFFFF;
  perf_timer.Init.Prescaler         = prescaler;
  perf_timer.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  perf_timer.Init.CounterMode       = TIM_COUNTERMODE_UP;
  perf_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if(HAL_TIM_Base_Init(&perf_timer) != HAL_OK)
    return;

  // Free running timer with no interrupts
  HAL_TIM_Base_Start(&perf_timer);
#endif
}
