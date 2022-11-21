#include <stdint.h>
#include <stdbool.h>

#include "build_config.h" // Get build-specific platform settings
#include "cstone/platform.h"
#include "lib_cfg/cstone_cfg_stm32.h"

#include "stm32f4xx_ll_rcc.h"

#include "cstone/core_stm32.h"


#define BUS_APB1 1
#define BUS_APB2 2

static int timer_bus(TIM_TypeDef *timer) {
/*
  Available timers:
    STM32F401  1, 2-5, 9-11
    STM32F429  1-14
    STM32F103  1-14
*/

  switch((uintptr_t)timer) {
  default: return BUS_APB1; break;

  case TIM1_BASE: return BUS_APB2; break;

  case TIM2_BASE: return BUS_APB1; break;
  case TIM3_BASE: return BUS_APB1; break;
  case TIM4_BASE: return BUS_APB1; break;
  case TIM5_BASE: return BUS_APB1; break;

#  if defined BOARD_STM32F429I_DISC1 || defined PLATFORM_STM32F1
  case TIM6_BASE: return BUS_APB1; break;
  case TIM7_BASE: return BUS_APB1; break;

  case TIM8_BASE: return BUS_APB2; break;
#  endif

  case TIM9_BASE:  return BUS_APB2; break;
  case TIM10_BASE: return BUS_APB2; break;
  case TIM11_BASE: return BUS_APB2; break;

#  if defined BOARD_STM32F429I_DISC1 || defined PLATFORM_STM32F1
  case TIM12_BASE: return BUS_APB1; break;
  case TIM13_BASE: return BUS_APB1; break;
  case TIM14_BASE: return BUS_APB1; break;
#  endif
  }
}


uint32_t timer_clock_rate(TIM_TypeDef *timer) {
  LL_RCC_ClocksTypeDef sys_clocks;
  LL_RCC_GetSystemClocksFreq(&sys_clocks);

  uint32_t timer_clk = (timer_bus(timer) == BUS_APB1) ?
                        sys_clocks.PCLK1_Frequency : sys_clocks.PCLK2_Frequency;

  // Timer clocks are 2x base APB clock rate unless APB prescaler is /1
  // When /1 is selected timer_clk == hclk, otherwise it's smaller
  if(timer_clk < sys_clocks.HCLK_Frequency) // APB prescaler > 1
    timer_clk *= 2;

  return timer_clk;
}


// Convert DMA periph and stream to matching IRQ
IRQn_Type stm32_dma_stream_irq(DMA_TypeDef *dma, unsigned stream) {
  static const IRQn_Type s_dma1_irqs[] = {
    DMA1_Stream0_IRQn, DMA1_Stream1_IRQn, DMA1_Stream2_IRQn, DMA1_Stream3_IRQn,
    DMA1_Stream4_IRQn, DMA1_Stream5_IRQn, DMA1_Stream6_IRQn, DMA1_Stream7_IRQn
  };

  static const IRQn_Type s_dma2_irqs[] = {
    DMA2_Stream0_IRQn, DMA2_Stream1_IRQn, DMA2_Stream2_IRQn, DMA2_Stream3_IRQn,
    DMA2_Stream4_IRQn, DMA2_Stream5_IRQn, DMA2_Stream6_IRQn, DMA2_Stream7_IRQn
  };

  if(dma == DMA1)
    return s_dma1_irqs[stream & 0x07];
  else
    return s_dma2_irqs[stream & 0x07];
}

