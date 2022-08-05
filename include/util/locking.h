#ifndef LOCKING_H
#define LOCKING_H

#if defined USE_FREERTOS
#  include "FreeRTOS.h"
#  include "task.h"

// Disable interrupts
#  define ENTER_CRITICAL() taskENTER_CRITICAL()
#  define EXIT_CRITICAL()  taskEXIT_CRITICAL()

#elif defined USE_NO_LOCK
#  define ENTER_CRITICAL()
#  define EXIT_CRITICAL()

#else
#  error "Locking primitives required for this platform"
#endif

#endif // LOCKING_H
