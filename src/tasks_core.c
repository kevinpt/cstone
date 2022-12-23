#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "build_config.h"
#include "cstone/platform.h"

#ifdef PLATFORM_HOSTED
#  include <unistd.h>
#endif


#include "FreeRTOS.h"
#include "timers.h"
#include "semphr.h"
#include "cstone/rtos.h"

#include "cstone/prop_id.h"
#include "cstone/prop_db.h"
#include "cstone/log_db.h"
#include "cstone/log_props.h"
#include "cstone/umsg.h"
#include "cstone/debug.h"
#include "cstone/error_log.h"
#include "cstone/console.h"
#include "cstone/led_blink.h"

#include "cstone/tasks_core.h"
#include "util/histogram.h"



UMsgHub g_msg_hub;

#ifdef USE_ERROR_MONITOR
extern ErrorLog g_error_log;
#endif
#ifdef USE_LOG_DB
extern PropDB   g_prop_db;
extern LogDB    g_log_db;
#endif

#ifdef USE_ERROR_MONITOR
static UMsgTarget s_tgt_error_monitor;
#endif
#ifdef USE_EVENT_MONITOR
static UMsgTarget s_tgt_event_monitor;
#endif
#ifdef USE_LOG_DB
static UMsgTarget s_tgt_prop_update;
static TaskHandle_t s_log_db_task;
#endif



// TASK: Umsg hub
static void umsg_hub_task(void *ctx) {
  while(1) {
    // Block and wait for messages to arrive
    umsg_hub_process_inbox(&g_msg_hub, 1);
  }
}


#ifdef USE_ERROR_MONITOR
// TASK: Error monitor
static void error_monitor_task(void *ctx) {
  UMsg msg;
  char buf[64];

  while(1) {
    // Block and wait for messages to arrive
    if(umsg_tgt_recv(&s_tgt_error_monitor, &msg, INFINITE_TIMEOUT)) {
      ErrorEntry entry = {.id = msg.id, .data = (uint32_t)msg.payload};
      errlog_write(&g_error_log, &entry);

      if((msg.id & P1_MSK) == P1_ERROR)
        fputs("\n" ERROR_PREFIX, stdout);
      else
        fputs("\n" WARN_PREFIX, stdout);

      printf(" " PROP_ID ", %s = %" PRId32 A_NONE "\n", msg.id,
            prop_get_name(msg.id, buf, sizeof(buf)), (int32_t)msg.payload);
      umsg_discard(&msg);
    }
  }
}
#endif

#ifdef USE_EVENT_MONITOR
// TASK: Event monitor
static void event_monitor_task(void *ctx) {
  UMsg msg;
  char buf[64];


  while(1) {
    // Block and wait for messages to arrive
    if(umsg_tgt_recv(&s_tgt_event_monitor, &msg, INFINITE_TIMEOUT)) {
      if(msg.id == (P1_DEBUG | P2_SYS | P3_LOCAL | P4_VALUE))
        debug_set_level(msg.payload);

      // Events are silent unless debugging is active
      if(DEBUG_IS_ON && (msg.id & P1_MSK) == P1_EVENT) {
        prop_get_name(msg.id, buf, sizeof(buf));
        printf("\n" A_BLU "EVENT: " PROP_ID " %s", msg.id, buf);

        if(msg.source != 0) {
          prop_get_name(msg.source, buf, sizeof(buf));
          printf(",  Src: " PROP_ID " %s", msg.source, buf);
        }

        if(msg.payload_size == 0) {
          printf(",  Val: %" PRIuPTR, msg.payload);
        }

        fputs(A_NONE, stdout);
      }

      umsg_discard(&msg);
    }
  }
}
#endif


#ifdef USE_LOAD_MONITOR

static Histogram *s_load_hist;
static uint32_t s_sys_load = 0;

void plot_load_stats(void) {
  puts("\n  System load %:");
  uint32_t max_bin = histogram_max_bin(s_load_hist);
  histogram_plot_horiz(s_load_hist, /*max_bar_len*/6, /*indent*/4, /*min_tick_step*/0,
    /*bar_threshold*/max_bin * 7 / 8);
}


uint32_t system_load(void) {
  return s_sys_load;
}


// TASK: Load monitor
static void load_monitor_task_cb(TimerHandle_t timer) {
  static uint32_t s_idle_time_total = 0;

  uint32_t idle_time = ulTaskGetIdleRunTimeCounter();
  uint32_t idle_ticks = idle_time - s_idle_time_total;
  s_idle_time_total = idle_time;

#if LOAD_MONITOR_TASK_MS != 1000
  // Get ticks per second
  idle_ticks = idle_ticks * 1000ul / LOAD_MONITOR_TASK_MS;
#endif

  // Calculate load in percent
  s_sys_load = (PERF_CLOCK_HZ - idle_ticks) * 100 / PERF_CLOCK_HZ;
#if 0
  fputs("## load: ", stdout);
  char buf[10+1];
  AppendRange rng = RANGE_FROM_ARRAY(buf);
  range_cat_uint(&rng, load);
  puts(buf);
#endif

  histogram_add_sample(s_load_hist, s_sys_load);
}
#endif


// Use an independent timestamp so multiple LEDs stay synchronized.
// This will only increment when the blink task runs.
static unsigned s_blink_timestamp = 0;

// Callback for blink handler
unsigned blink_timestamp(void) {
  return s_blink_timestamp;
}

// TASK: Schedule LED blinks
#ifdef USE_LED_BLINK_PERIODIC_TASK
static void blink_task_cb(void *ctx) {
#else
static void blink_task_cb(TimerHandle_t timer) {
#endif
  blinkers_update_all();
  s_blink_timestamp += BLINK_TASK_MS;
}

#ifdef USE_CONSOLE_TASK
// TASK: Console
// FIXME: Consider adding a task notification for handling nearly full RX queues
static void console_task_cb(void *ctx) {
  // Process RX data on all consoles
  Console *cur = first_console();

  while(cur) {
    task_set_console(NULL, cur);
    shell_process_rx(cur);
    cur = cur->next;
  }
}


#  ifdef PLATFORM_HOSTED
// TASK: Stdin read
// Feed stdin into the console queue
static void stdin_read_task(void *ctx) {
  Console *con = active_console();

  while(1) {
    char buf[16];
    int count = read(STDIN_FILENO, buf, sizeof buf);
    if(count > 0)
      console_rx_enqueue(con, (uint8_t *)buf, count);
  }
}
#  endif

#endif


#ifdef USE_LOG_DB
// TASK: Log DB update
static void log_db_task_cb(void *ctx) {
  static unsigned s_log_update_timeout = 0;

  if(s_log_update_timeout == 0) {
    // Block waiting for notification that PROP_UPDATE event triggered
    ulTaskNotifyTake(/*xClearCountOnExit*/ pdTRUE, portMAX_DELAY);

    // Start timeout
    s_log_update_timeout = (LOG_DB_TASK_DELAY_MS / LOG_DB_TASK_MS) + 1;

    update_prng_seed(&g_prop_db); // Generate a new seed

    // Increment write counter
    PropDBEntry entry;
    if(prop_get(&g_prop_db, P_SYS_STORAGE_INFO_COUNT, &entry)) {
      prop_set_uint(&g_prop_db, P_SYS_STORAGE_INFO_COUNT, entry.value + 1, 0);
      prop_set_attributes(&g_prop_db, P_SYS_STORAGE_INFO_COUNT, P_PROTECT | P_PERSIST);
    }

  } else {  // Timeout is active
    if(--s_log_update_timeout > 0) // Not expired
      return;

    DPUTS("Update prop log");

    // Clear any new notification since timeout began
    ulTaskNotifyTake(/*xClearCountOnExit*/ pdTRUE, 0);
    save_props_to_log(&g_prop_db, &g_log_db, /*compress*/true);
  }
}

// Callback for msg hub on prop update
static void prop_update_handler(UMsgTarget *tgt, UMsg *msg) {
  xTaskNotifyGive(s_log_db_task); // Start LogDB task timeout
}
#endif


void core_tasks_init(void) {
  // ** Normal tasks **

  xTaskCreate(umsg_hub_task, "MsgHub", STACK_BYTES(2048),
              NULL, TASK_PRIO_LOW, NULL);

#ifdef USE_ERROR_MONITOR
  xTaskCreate(error_monitor_task, "ErrorMon", STACK_BYTES(1024),
              NULL, TASK_PRIO_LOW, NULL);

  // Monitor all errors
  umsg_tgt_queued_init(&s_tgt_error_monitor, 4);
  umsg_tgt_add_filter(&s_tgt_error_monitor, (P1_ERROR | P2_MSK | P3_MSK | P4_MSK));
  umsg_tgt_add_filter(&s_tgt_error_monitor, (P1_WARN | P2_MSK | P3_MSK | P4_MSK));
  umsg_hub_subscribe(&g_msg_hub, &s_tgt_error_monitor);
#endif

#ifdef USE_EVENT_MONITOR
  xTaskCreate(event_monitor_task, "EventMon", STACK_BYTES(2048+300),
              NULL, TASK_PRIO_LOW, NULL);

  // Monitor all events and debug messages
  umsg_tgt_queued_init(&s_tgt_event_monitor, 4);
  umsg_tgt_add_filter(&s_tgt_event_monitor, (P1_EVENT | P2_MSK | P3_MSK | P4_MSK));
  umsg_tgt_add_filter(&s_tgt_event_monitor, (P1_DEBUG | P2_MSK | P3_MSK | P4_MSK));
  umsg_hub_subscribe(&g_msg_hub, &s_tgt_event_monitor);
#endif

  // ** Timer tasks **

#ifdef USE_LOAD_MONITOR
  s_load_hist = histogram_init(50, 0, 100, /*track_overflow*/false);

  TimerHandle_t load_timer = xTimerCreate(  // System load monitor
    "LOAD",
    LOAD_MONITOR_TASK_MS,
    pdTRUE, // uxAutoReload
    NULL,   // pvTimerID
    load_monitor_task_cb
  );

  xTimerStart(load_timer, 0);
#endif

#ifdef USE_LED_BLINK_PERIODIC_TASK
  // Run BlinkLED as an independent task
  static PeriodicTaskCfg cfg = {  // LED blink handler
    .task = blink_task_cb,
    .ctx = NULL,
    .period = BLINK_TASK_MS,
    .repeat = REPEAT_FOREVER
  };

  create_periodic_task("BlinkLED", STACK_BYTES(256), TASK_PRIO_LOW, &cfg);

#else
  // Run BlinkLED as a timer task
  TimerHandle_t led_timer = xTimerCreate(  // LED blink handler
    "BlinkLED",
    BLINK_TASK_MS,
    pdTRUE, // uxAutoReload
    NULL,   // pvTimerID
    blink_task_cb
  );

  xTimerStart(led_timer, 0);
#endif

#ifdef USE_CONSOLE_TASK
  static PeriodicTaskCfg console_task_cfg = { // Console command proc
    .task   = console_task_cb,
    .ctx    = NULL,
    .period = CONSOLE_TASK_MS,
    .repeat = REPEAT_FOREVER
  };

  create_periodic_task("Con", STACK_BYTES(2048), TASK_PRIO_LOW, &console_task_cfg);

#  ifdef PLATFORM_HOSTED
  xTaskCreate(stdin_read_task, "stdin", STACK_BYTES(1024),
              NULL, TASK_PRIO_LOW, NULL);
#  endif
#endif

#ifdef USE_LOG_DB
  static PeriodicTaskCfg log_db_task_cfg = { // Log DB updates
    .task   = log_db_task_cb,
    .ctx    = NULL,
    .period = LOG_DB_TASK_MS,
    .repeat = REPEAT_FOREVER
  };

  s_log_db_task = create_periodic_task("LogDB", STACK_BYTES(2304), TASK_PRIO_LOW, &log_db_task_cfg);

  // Handle property transactions
  umsg_tgt_callback_init(&s_tgt_prop_update, prop_update_handler);
  umsg_tgt_add_filter(&s_tgt_prop_update, (P1_EVENT | P2_STORAGE | P3_PROP | P4_UPDATE));
  umsg_hub_subscribe(&g_msg_hub, &s_tgt_prop_update);
#endif
}

