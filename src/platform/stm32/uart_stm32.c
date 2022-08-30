#include <stdint.h>
#include <stdbool.h>

#include "build_config.h" // Get build-specific platform settings

#if defined PLATFORM_STM32F1
#  include "stm32f1xx_ll_usart.h"
#else
#  include "stm32f4xx_ll_usart.h"
#endif

#include "FreeRTOS.h"

#include "cstone/io/gpio.h"
#include "cstone/io/uart_stm32.h"


#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif


typedef struct {
  USART_TypeDef  *dev;
  IRQn_Type       irq;
} UartResource;

static const UartResource s_uart_resources[] = {
  {USART1, USART1_IRQn},
  {USART2, USART2_IRQn},
  {USART3, USART3_IRQn},
#ifdef UART4
  {UART4,  UART4_IRQn},
#endif
#ifdef UART5
  {UART5,  UART5_IRQn},
#endif
#ifdef USART6
  {USART6, USART6_IRQn},
#endif
#ifdef UART7
  {UART7,  UART7_IRQn},
#endif
#ifdef UART8
  {UART8,  UART8_IRQn}
#endif
};

static inline const UartResource *uart__get_resource(int id) {
  if(id < 1 || id > (int)COUNT_OF(s_uart_resources))
    id = 1;

  return &s_uart_resources[id-1];
}


// NOTE: This returns void * so that console_uart.c doesn't have to include any HAL headers
void *uart_get_device(int id) {
  return uart__get_resource(id)->dev;
}


static void uart__clk_enable(int id) {
  switch(id) {
  default:
  case 1: __HAL_RCC_USART1_CLK_ENABLE(); break;
  case 2: __HAL_RCC_USART2_CLK_ENABLE(); break;
  case 3: __HAL_RCC_USART3_CLK_ENABLE(); break;
#ifdef UART4
  case 4: __HAL_RCC_UART4_CLK_ENABLE(); break;
#endif
#ifdef UART5
  case 5: __HAL_RCC_UART5_CLK_ENABLE(); break;
#endif
#ifdef USART6
  case 6: __HAL_RCC_USART6_CLK_ENABLE(); break;
#endif
#ifdef UART7
  case 7: __HAL_RCC_UART7_CLK_ENABLE(); break;
#endif
#ifdef UART8
  case 8: __HAL_RCC_UART8_CLK_ENABLE(); break;
#endif
  }
}


__attribute__((weak))
void uart_io_init(void) {}


void uart_init(int id, uint8_t port, uint32_t baud) {
  uart__clk_enable(id);
  gpio_enable_port(port); // FIXME: Not really necessary

  // Configure peripheral
  LL_USART_InitTypeDef uart_cfg = {0};
  uart_cfg.BaudRate             = baud;
  uart_cfg.DataWidth            = LL_USART_DATAWIDTH_8B;
  uart_cfg.StopBits             = LL_USART_STOPBITS_1;
  uart_cfg.Parity               = LL_USART_PARITY_NONE;
  uart_cfg.TransferDirection    = LL_USART_DIRECTION_TX_RX;
  uart_cfg.HardwareFlowControl  = LL_USART_HWCONTROL_NONE;
  uart_cfg.OverSampling         = LL_USART_OVERSAMPLING_16;

  const UartResource *uart_rsrc = uart__get_resource(id);
  LL_USART_Init(uart_rsrc->dev, &uart_cfg);
  LL_USART_ConfigAsyncMode(uart_rsrc->dev);

  // Configure interrupts
  // NOTE: UART priority must not exceed (be less than) max for syscall so we can give
  //       the tx_empty semaphore in the ISR.
  HAL_NVIC_SetPriority(uart_rsrc->irq, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(uart_rsrc->irq);

  LL_USART_EnableIT_RXNE(uart_rsrc->dev);
  LL_USART_DisableIT_TC(uart_rsrc->dev);
  LL_USART_DisableIT_TXE(uart_rsrc->dev);


  LL_USART_Enable(uart_rsrc->dev);

}


void uart_send_enable(int id) {
  const UartResource *uart = uart__get_resource(id);
  LL_USART_DisableIT_TC(uart->dev);
  LL_USART_EnableIT_TXE(uart->dev);
}

