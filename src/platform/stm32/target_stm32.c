#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "cstone/platform.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#include "cstone/target.h"

void software_reset(void) {
#ifdef PLATFORM_EMBEDDED
  puts("\nResetting...");
  vTaskDelay(pdMS_TO_TICKS(200)); // Provide time for stdout to flush
  NVIC_SystemReset();
#else
  puts("Simulator can't reset");
#endif
}
