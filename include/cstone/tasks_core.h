#ifndef TASKS_CORE_H
#define TASKS_CORE_H

// ******************** Configuration ********************
#define USE_LOAD_MONITOR
#define USE_CONSOLE_TASK

#if !defined USE_MINIMAL_TASKS || !USE_MINIMAL_TASKS
#  define USE_ERROR_MONITOR
#  define USE_EVENT_MONITOR
#  define USE_LOG_DB
#endif


//#define USE_LED_BLINK_PERIODIC_TASK


extern UMsgHub g_msg_hub;


#define LOAD_MONITOR_TASK_MS    1000
#define BLINK_TASK_MS           40    // Update LEDs
#define EVENT_SEQUENCER_TASK_MS 50
#define CONSOLE_TASK_MS         17    // Process RX data
#define LOG_DB_TASK_MS          50

#define LOG_DB_TASK_DELAY_MS  1000

#ifdef __cplusplus
extern "C" {
#endif

void core_tasks_init(void);
#ifdef USE_LOAD_MONITOR
void plot_load_stats(void);
uint32_t system_load(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // TASKS_CORE_H
