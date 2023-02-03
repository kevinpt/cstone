#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h> // For mallinfo()
#include <string.h>

#include "build_config.h" // Get build-specific platform settings
#include "cstone/platform.h"

#if defined PLATFORM_STM32
#  include <unistd.h> // For sbrk()
#  define STM32_HAL_LEGACY  // Inhibit legacy header
#  if defined PLATFORM_STM32F1
#    include "stm32f1xx_hal.h"
#  else
#    include "stm32f4xx_hal.h"
#  endif

#else // Hosted
#  include "cstone/timing.h"
#endif

#include "FreeRTOS.h"
#include "timers.h"
#include "cstone/rtos.h"
#include "cstone/prop_id.h"
#include "cstone/debug.h"

#include "util/mempool.h"
#include "util/range_strings.h"

extern void fatal_error(void);


// FreeRTOS heap storage
#if configAPPLICATION_ALLOCATED_HEAP == 1
#  ifdef PLATFORM_EMBEDDED
__attribute__(( section(FREERTOS_HEAP_SECTION) ))
#  endif
_Alignas(max_align_t)
uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif


// FreeRTOS callbacks

void vApplicationIdleHook(void);
void vApplicationDaemonTaskStartupHook(void);

void vApplicationIdleHook(void) {
#ifdef PLATFORM_EMBEDDED
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
#endif
}


// Called when configUSE_MALLOC_FAILED_HOOK enabled
void vApplicationMallocFailedHook(void) {
  puts("\nERROR: FreeRTOS malloc failed");
  fatal_error();
}

// Called when configCHECK_FOR_STACK_OVERFLOW == 1 or 2
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName) {
  puts("\nERROR: Stack overflow");
  fatal_error();
}

extern void ui_set_default(void);

// Called once when starting timer task for init with scheduler running
void vApplicationDaemonTaskStartupHook(void) {
//  ui_set_default();
}


#ifdef configSUPPORT_STATIC_ALLOCATION
// TinyUSB requires static FreeRTOS objects so we add the necessary allocation
// callbacks for the builtin tasks.

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize ) {
  /* If the buffers to be provided to the Idle task are declared inside this
  function then they must be declared static - otherwise they will be allocated on
  the stack and so not exists after this function exits. */
  static StaticTask_t xIdleTaskTCB;
  static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {

  /* If the buffers to be provided to the Timer task are declared inside this
  function then they must be declared static - otherwise they will be allocated on
  the stack and so not exists after this function exits. */
  static StaticTask_t xTimerTaskTCB;
  static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

  /* Pass out a pointer to the StaticTask_t structure in which the Timer
  task's state will be stored. */
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

  /* Pass out the array that will be used as the Timer task's stack. */
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;

  /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif // configSUPPORT_STATIC_ALLOCATION


static void report_task_stack_usage(mpPoolSet *pool_set) {
  // Allocate storage from pool to minimize stack usage
  struct pool_obj {
    TaskStatus_t task_info;
    AppendRange rng;
    char buf[8];
  };

  struct pool_obj *ctx = mp_alloc(pool_set, sizeof (struct pool_obj), NULL);
  if(ctx) {
    vTaskGetInfo(NULL, &ctx->task_info, /*xGetFreeStackSpace*/pdTRUE, eInvalid);

    // We're running in a potentially small stack so printf() can't be used
    fputs(FLAG_PREFIX(REPORTSTACK) " '", stdout);
    fputs(ctx->task_info.pcTaskName, stdout);
    fputs("'  min. stack = ", stdout);
    range_init(&ctx->rng, ctx->buf, sizeof ctx->buf);
    range_cat_uint(&ctx->rng, ctx->task_info.usStackHighWaterMark * sizeof (StackType_t));
    fputs(ctx->buf, stdout);
    puts(A_NONE);

    mp_free(pool_set, ctx);
  }
}


static void periodic_task_wrapper(void *param) {
  PeriodicTaskCfg *cfg = (PeriodicTaskCfg *)param;

  TickType_t period_ticks = pdMS_TO_TICKS(cfg->period);
  TickType_t prev_wake = xTaskGetTickCount();

  if(cfg->start_delay > 0) {
    vTaskDelayUntil(&prev_wake, pdMS_TO_TICKS(cfg->start_delay));
  }

  while(1) {  // Run task continuously
    TickType_t now = xTaskGetTickCount();

    // Correct wake time if we've been suspended for more than the task period
    if((now - prev_wake) >= period_ticks)
      prev_wake = now;

    cfg->task(cfg->ctx);


    if(cfg->repeat > 0)
      cfg->repeat--;

    if(cfg->repeat == 0)  // Terminate
      break;

    vTaskDelayUntil(&prev_wake, period_ticks);
  }

  mpPoolSet *pool_set = mp_sys_pools();
  mp_free(pool_set, cfg);

  if(DEBUG_FEATURE(PF_DEBUG_SYS_LOCAL_REPORTSTACK))
    report_task_stack_usage(pool_set);

  vTaskDelete(NULL);
}


TaskHandle_t create_periodic_task(const char *name, configSTACK_DEPTH_TYPE stack,
                                  UBaseType_t priority, PeriodicTaskCfg *cfg) {

  TaskHandle_t handle = NULL;

  if(cfg->repeat == 0)
    cfg->repeat = 1;

  // Copy config struct into allocated object. This lets us use a cfg passed from
  // the caller's stack.
  mpPoolSet *pool_set = mp_sys_pools();
  PeriodicTaskCfg *own_cfg = mp_alloc(pool_set, sizeof *cfg, NULL);
  if(!own_cfg)
    return NULL;
  memcpy(own_cfg, cfg, sizeof *cfg);

  BaseType_t status = xTaskCreate(periodic_task_wrapper, name, stack, own_cfg, priority, &handle);

  if(status != pdPASS) {
    mp_free(pool_set, own_cfg);
    return NULL;
  }

  return handle;
}


// Retrieve heap stats

// Linker symbols
extern char __heap_start;
extern char __heap_end;


size_t heap_c_lib_size(void) {
#ifdef PLATFORM_EMBEDDED
  return &__heap_end - &__heap_start;
#else
  struct mallinfo mi = mallinfo();
  return mi.arena;
#endif
}

size_t heap_c_lib_free(void) {
#ifdef PLATFORM_EMBEDDED
  struct mallinfo mi = mallinfo();
  size_t heap_after_break = &__heap_end - (char *)sbrk(0);

  return mi.fordblks + heap_after_break;
#else
  struct mallinfo mi = mallinfo();
  return mi.fordblks;
#endif
}



size_t heap_os_size(void) {
  return configTOTAL_HEAP_SIZE;
}

size_t heap_os_free(void) {
  return xPortGetFreeHeapSize();
}

size_t heap_os_min_free(void) {
  return xPortGetMinimumEverFreeHeapSize();
}

size_t heap_os_allocated_objs(void) {
  HeapStats_t stats;
  vPortGetHeapStats(&stats);

  return stats.xNumberOfSuccessfulAllocations - stats.xNumberOfSuccessfulFrees;
}

// FIXME: Move to stm32 platform code
#ifndef PLATFORM_HOSTED

// Linker symbols
extern uint32_t _estack;
extern uint32_t _reserved_stack_size;

size_t sys_stack_size(void) {
  return (uint32_t)&_reserved_stack_size;
}


#define STACK_SENTINEL 0xDEADBEEF

// This should be called early in main() and it must not be called from an RTOS task
void sys_stack_fill(void) {
  uint32_t *end_stack = &_estack;
  uint32_t *start_stack = end_stack - sys_stack_size();
  end_stack = (uint32_t *)__get_MSP();
  uint32_t *top_stack = start_stack;

  while(top_stack < end_stack) {
    *top_stack++ = STACK_SENTINEL;
  }

}

size_t sys_stack_min_free(void) {
  uint32_t *end_stack = &_estack;
  uint32_t *start_stack = end_stack - sys_stack_size();
  uint32_t *top_stack = start_stack;

  while(*top_stack == STACK_SENTINEL) {
    if(++top_stack >= end_stack)
      break;
  }

  return top_stack - start_stack;
}
#endif
