#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "cstone/platform.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_rcc.h"

#include "FreeRTOS.h"
#include "task.h"

#include "cstone/blocking_io.h"
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


ResetSource get_reset_source(void) {
  uint32_t reset = RCC->CSR;

  // Decode reset bits in the RCC CSR into a simpler enumeration

  if(reset & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF))  return RESET_WATCHDOG;
  if(reset & RCC_CSR_LPWRRSTF) return RESET_SLEEP;

  if(reset & RCC_CSR_PORRSTF) return RESET_POWERON;
  // Brown out reset is also triggered by power on so this must come after that check
  if(reset & RCC_CSR_BORRSTF) return RESET_BROWNOUT;

  if(reset & RCC_CSR_SFTRSTF) return RESET_SOFTWARE;

  // Pin reset is triggered by other reset sources so it must be checked last (RM0090 Fig. 15)
  if(reset & RCC_CSR_PINRSTF) return RESET_PIN;

  return RESET_UNKNOWN;
}


void report_reset_source(void) {
  fputs("Reset source:  ", stdout);
  int items = 0;
#define COMMA() if(items > 0) fputs(", ", stdout)

  // Check all flags so we can report when multiples are set at once

  if(LL_RCC_IsActiveFlag_LPWRRST()) {fputs("Low power", stdout); items++;}
  if(LL_RCC_IsActiveFlag_WWDGRST()) {COMMA(); fputs("WWDG", stdout); items++;}
  if(LL_RCC_IsActiveFlag_IWDGRST()) {COMMA(); fputs("IWDG", stdout); items++;}
  if(LL_RCC_IsActiveFlag_SFTRST())  {COMMA(); fputs("Software", stdout); items++;}
  if(LL_RCC_IsActiveFlag_PORRST())  {COMMA(); fputs("Power on/down", stdout); items++;}
  if(LL_RCC_IsActiveFlag_BORRST())  {COMMA(); fputs("Brownout", stdout);}
  if(LL_RCC_IsActiveFlag_PINRST())  {COMMA(); fputs("NRST pin", stdout); items++;}
  putnl();
}
