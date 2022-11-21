#ifndef CORE_STM32_H
#define CORE_STM32_H


#ifdef __cplusplus
extern "C" {
#endif

uint32_t timer_clock_rate(TIM_TypeDef *timer);
IRQn_Type stm32_dma_stream_irq(DMA_TypeDef *dma, unsigned stream);

#ifdef __cplusplus
}
#endif

#endif // CORE_STM32_H
