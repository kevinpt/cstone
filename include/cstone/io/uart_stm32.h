#ifndef UART_STM32_H
#define UART_STM32_H

#ifdef __cplusplus
extern "C" {
#endif

void uart_io_init(void);
void uart_init(int id, uint8_t port, uint32_t baud);

void *uart_get_device(int id);  // Returns a USART_TypeDef *

void uart_send_enable(int id);

#ifdef __cplusplus
}
#endif

#endif // UART_STM32_H
