#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/console.h"

#include "cstone/io/uart.h"
#include "cstone/console_uart.h"


// Callback for Console object
static void uart_send(DualStream *stream) {
  Console *con = (Console *)stream->io_ctx;
  // Start UART IRQHandler
  uart_send_enable(con->id.id);
}


bool uart_console_init(int uart_id, ConsoleConfigBasic *cfg) {

  Console *con = console_alloc(cfg);
  if(con) {
    con->stream.io_send = uart_send;
    con->stream.io_ctx = con;
    con->id = (ConsoleID){.kind = CON_UART, .id = uart_id};
    console_add(con);

    return true;
  }

  return false;
}
