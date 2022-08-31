#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/console.h"
#include "cstone/console_usb.h"

extern TaskHandle_t g_usb_cdc_task; // FIXME: Move to cstone header


// Callback for Console object
static void usb_send(DualStream *stream) {
  xTaskNotifyGive(g_usb_cdc_task);
}


bool usb_console_init(int usb_id, ConsoleConfigBasic *cfg) {

  Console *con = console_alloc(cfg);
  if(con) {
    con->stream.io_send = usb_send;
    con->id = (ConsoleID){.kind = CON_USB, .id = usb_id};
    console_add(con);

    return true;
  }

  return false;
}

