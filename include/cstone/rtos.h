#ifndef RTOS_H
#define RTOS_H

#include "task.h"
#include "cstone/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PLATFORM_EMBEDDED
// On POSIX port, IDLE task will never run if other tasks have higher priority
// That prevents cleanup of OS resources
#  define TASK_PRIO_LOW   (tskIDLE_PRIORITY + 0)
#  define TASK_PRIO_HIGH  (tskIDLE_PRIORITY + 0)

#else // PLATFORM_EMBEDDED
#  define TASK_PRIO_LOW   (tskIDLE_PRIORITY + 1)
#  define TASK_PRIO_HIGH  (tskIDLE_PRIORITY + 4)
#endif


// Convert bytes to words for task stack size arguments
#define STACK_BYTES(b)  (((b) + sizeof(StackType_t)-1) / sizeof(StackType_t))


typedef void (*PeriodicTask)(void *ctx);

#define REPEAT_FOREVER  (-1)

typedef struct {
  PeriodicTask  task;
  void         *ctx;
  uint32_t      period;       // Milliseconds
  int32_t       repeat;
  uint32_t      start_delay;  // Milliseconds
} PeriodicTaskCfg;


TaskHandle_t create_periodic_task(const char *name, configSTACK_DEPTH_TYPE stack,
                                  UBaseType_t priority, PeriodicTaskCfg *cfg);

static inline TaskHandle_t create_delayed_task(PeriodicTask task, void *ctx, configSTACK_DEPTH_TYPE stack,
                                  uint32_t start_delay) {
  PeriodicTaskCfg cfg = {
    .task   = task,
    .ctx    = ctx,
    .period = 0,
    .repeat = 1,
    .start_delay = start_delay
  };
  return create_periodic_task("delay", stack, TASK_PRIO_LOW, &cfg);
}

#ifndef PERF_TIMER_DECL
// FreeRTOS Performance timer callbacks
void perf_timer_init(void);
uint32_t perf_timer_count(void);
#endif

size_t heap_c_lib_size(void);
size_t heap_c_lib_free(void);

size_t heap_os_size(void);
size_t heap_os_free(void);
size_t heap_os_min_free(void);
size_t heap_os_allocated_objs(void);

void sys_stack_fill(void);
size_t sys_stack_size(void);
size_t sys_stack_min_free(void);

#ifdef __cplusplus
}
#endif

#endif // RTOS_H
