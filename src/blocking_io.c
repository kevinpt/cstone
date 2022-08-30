#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "build_config.h"
#include "cstone/platform.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/console.h"
#include "cstone/blocking_io.h"


// ******************** Blocking print ********************

int bprintf(const char *fmt, ...) {
  va_list args;

#ifdef PLATFORM_EMBEDDED
  Console *con = active_console();
  if(con)
    xSemaphoreTake(con->tx_empty, portMAX_DELAY); // Block until TX queue is empty
#endif

  va_start(args, fmt);
  int status = vprintf(fmt, args);
  va_end(args);

  return status;
}


int bputs(const char *str) {
#ifdef PLATFORM_EMBEDDED
  Console *con = active_console();
  if(con)
    xSemaphoreTake(con->tx_empty, portMAX_DELAY); // Block until TX queue is empty
#endif

  return puts(str);
}


int bfputs( const char *str, FILE *stream) {
#ifdef PLATFORM_EMBEDDED
  Console *con = active_console();
  if(con)
    xSemaphoreTake(con->tx_empty, portMAX_DELAY); // Block until TX queue is empty
#endif

  return fputs(str, stream);
}


