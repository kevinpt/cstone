#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/isr_queue.h"
#include "cstone/dual_stream.h"


void dualstream_init(DualStream *stream, DualStreamConfig *cfg) {
  stream->tx_queue     = cfg->tx_queue;
  stream->rx_queue     = cfg->rx_queue;
  stream->io_send      = cfg->io_send;
  stream->io_ctx       = cfg->io_ctx;

  stream->tx_lock = xSemaphoreCreateBinary();
  xSemaphoreGive(stream->tx_lock);

  stream->rx_lock = xSemaphoreCreateBinary();
  xSemaphoreGive(stream->rx_lock);

  stream->tx_empty = xSemaphoreCreateBinary();
  xSemaphoreGive(stream->tx_empty);
}
