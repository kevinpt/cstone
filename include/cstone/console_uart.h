#ifndef CONSOLE_UART_H
#define CONSOLE_UART_H

#ifdef __cplusplus
extern "C" {
#endif

bool uart_console_init(int uart_id, ConsoleConfigBasic *cfg);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_UART_H
