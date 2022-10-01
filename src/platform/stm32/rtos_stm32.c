#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "build_config.h" // Get build-specific platform settings
#include "cstone/platform.h"
#include "lib_cfg/cstone_cfg_stm32.h"

#if defined PLATFORM_STM32F1
#  include "stm32f1xx_ll_tim.h"
#else
#  include "stm32f4xx_ll_tim.h"
#endif

#include "FreeRTOS.h"
#include "timers.h"

#include "cstone/rtos.h"


uint32_t perf_timer_count(void) {
#if defined PLATFORM_STM32F4
  return LL_TIM_GetCounter(PERF_TIMER);

#elif defined PLATFORM_STM32F1
  // Chained 16-bit timers must be merged into 32-bits
  uint16_t old_lsh  = LL_TIM_GetCounter(PERF_TIMER);
  uint32_t msh      = LL_TIM_GetCounter(PERF_TIMER_HI);
  uint16_t new_lsh  = LL_TIM_GetCounter(PERF_TIMER);
  if(new_lsh < old_lsh) // Rollover while reading
    msh = LL_TIM_GetCounter(PERF_TIMER_HI);

  return (msh << 16) | new_lsh;

#else
  return micros() / (1000000ul / PERF_CLOCK_HZ);
#endif
}


// Initialize free running timer with 100us resolution for run time stats
void perf_timer_init(void) {
#if defined PLATFORM_STM32F4
  PERF_TIMER_CLK_ENABLE();

  // TIM2 is on APB1 @ 45MHz with /4 prescaler. Timer clock is 2x @ 90MHz (180MHz / 2)
  uint16_t prescaler = (uint32_t) ((SystemCoreClock / 2) / PERF_CLOCK_HZ) - 1;

  LL_TIM_InitTypeDef tim_cfg;

  LL_TIM_StructInit(&tim_cfg);
  tim_cfg.Autoreload    = 0xFFFFFFFF;
  tim_cfg.Prescaler     = prescaler;

  LL_TIM_SetCounter(PERF_TIMER, 0);
  LL_TIM_Init(PERF_TIMER, &tim_cfg);
  LL_TIM_EnableCounter(PERF_TIMER);


#elif defined PLATFORM_STM32F1
  // STM32F1 only has 16-bit timers. We chain two together to get a 32-bit count

  LL_TIM_InitTypeDef tim_cfg;

  // **** Configure timer for low 16-bits ****
  PERF_TIMER_CLK_ENABLE();

  // Base clock is HCLK
  // Timer TIM2 is on APB1 @ 36MHz
  uint16_t prescaler = (uint32_t) ((SystemCoreClock / 1) / PERF_CLOCK_HZ) - 1;

  LL_TIM_StructInit(&tim_cfg);
  tim_cfg.Autoreload    = 0xFFFF;
  tim_cfg.Prescaler = prescaler;
  LL_TIM_Init(PERF_TIMER, &tim_cfg);

  LL_TIM_SetTriggerOutput(PERF_TIMER, LL_TIM_TRGO_UPDATE); // Clock high 16-bit timer on overflow

//////////////
// Experiment with output of timer channel
#if 0
  LL_TIM_OC_InitTypeDef oc_cfg;
  LL_TIM_OC_StructInit(&oc_cfg);
  oc_cfg.OCMode = LL_TIM_OCMODE_TOGGLE;
//  oc_cfg.OCState = LL_TIM_OCSTATE_ENABLE;
//  oc_cfg.OCNState = LL_TIM_OCSTATE_ENABLE;
  oc_cfg.CompareValue = 0xFF; //0xFFFF;

  LL_TIM_OC_Init(PERF_TIMER, LL_TIM_CHANNEL_CH4, &oc_cfg);
  LL_TIM_OC_EnablePreload(PERF_TIMER, LL_TIM_CHANNEL_CH4);
  LL_TIM_OC_ConfigR, LL_TIM_CHANNEL_CH4, LL_TIM_OCPOLARITY_HIGH | LL_TIM_OCIDLESTATE_HIGH);
  LL_TIM_EnableAllOutputs(PERF_TIMER);

  LL_TIM_CC_EnableChannel(PERF_TIMER, LL_TIM_CHANNEL_CH4);
//  LL_TIM_OC_SetMode(PERF_TIMER, LL_TIM_CHANNEL_CH4, LL_TIM_OCMODE_TOGGLE);
  LL_TIM_GenerateEvent_UPDATE(PERF_TIMER);
#endif
//////////////

  LL_TIM_SetCounter(PERF_TIMER, 0);
  LL_TIM_EnableCounter(PERF_TIMER);



  // **** Configure timer for high 16-bits ****
  PERF_TIMER_HI_CLK_ENABLE();

  LL_TIM_StructInit(&tim_cfg);
  tim_cfg.Autoreload    = 0xFFFF;
  tim_cfg.Prescaler = 0;
  LL_TIM_Init(PERF_TIMER_HI, &tim_cfg);

  LL_TIM_SetTriggerInput(PERF_TIMER_HI, LL_TIM_TS_ITR1); // Trigger from TIM2 (Table 82 in RM0008)
  LL_TIM_SetClockSource(PERF_TIMER_HI, LL_TIM_CLOCKSOURCE_EXT_MODE1);

  LL_TIM_SetCounter(PERF_TIMER_HI, 0);
  LL_TIM_EnableCounter(PERF_TIMER_HI);

#endif
}


