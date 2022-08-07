#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

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

extern ErrorLog g_error_log;
extern PropDB   g_prop_db;
extern LogDB    g_log_db;

static UMsgTarget s_tgt_error_monitor;
static UMsgTarget s_tgt_event_monitor;
static UMsgTarget s_tgt_prop_update;

static TaskHandle_t s_log_db_task;



// TASK: Umsg hub
static void umsg_hub_task(void *ctx) {
  while(1) {
    // Block and wait for messages to arrive
    umsg_hub_process_inbox(&g_msg_hub, 1);
  }
}


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



#ifdef USE_LOAD_MONITOR

static Histogram *s_load_hist;

void plot_load_stats(void) {
  puts("  System load %:");
  histogram_plot(s_load_hist, 50);
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
  uint32_t load = (PERF_CLOCK_HZ - idle_ticks) * 100 / PERF_CLOCK_HZ;
#if 0
  fputs("## load: ", stdout);
  char buf[10+1];
  AppendRange rng = RANGE_FROM_ARRAY(buf);
  range_cat_uint(&rng, load);
  puts(buf);
#endif

  histogram_add_sample(s_load_hist, load);
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
static void blink_task_cb(void *ctx) {
  blinkers_update_all();
  s_blink_timestamp += BLINK_TASK_MS;
}


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


// TASK: Log DB update
static void log_db_task_cb(void *ctx) {
  static unsigned s_log_update_timeout = 0;

  if(s_log_update_timeout == 0) {
    // Block waiting for notification that PROP_UPDATE event triggered
    ulTaskNotifyTake(/*xClearCountOnExit*/ pdTRUE, portMAX_DELAY);

    // Start timeout
    s_log_update_timeout = (LOG_DB_TASK_DELAY_MS / LOG_DB_TASK_MS) + 1;

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



void core_tasks_init(void) {
  // ** Normal tasks **

  xTaskCreate(umsg_hub_task, "MsgHub", STACK_BYTES(4096),
              NULL, TASK_PRIO_LOW, NULL);


  xTaskCreate(error_monitor_task, "ErrorMon", STACK_BYTES(1024),
              NULL, TASK_PRIO_LOW, NULL);

  // Monitor all errors
  umsg_tgt_queued_init(&s_tgt_error_monitor, 4);
  umsg_tgt_add_filter(&s_tgt_error_monitor, (P1_ERROR | P2_MSK | P3_MSK | P4_MSK));
  umsg_tgt_add_filter(&s_tgt_error_monitor, (P1_WARN | P2_MSK | P3_MSK | P4_MSK));
  umsg_hub_subscribe(&g_msg_hub, &s_tgt_error_monitor);


  xTaskCreate(event_monitor_task, "EventMon", STACK_BYTES(2048+300),
              NULL, TASK_PRIO_LOW, NULL);

  // Monitor all events and debug messages
  umsg_tgt_queued_init(&s_tgt_event_monitor, 4);
  umsg_tgt_add_filter(&s_tgt_event_monitor, (P1_EVENT | P2_MSK | P3_MSK | P4_MSK));
  umsg_tgt_add_filter(&s_tgt_event_monitor, (P1_DEBUG | P2_MSK | P3_MSK | P4_MSK));
  umsg_hub_subscribe(&g_msg_hub, &s_tgt_event_monitor);


  // ** Timer tasks **

#ifdef USE_LOAD_MONITOR
  s_load_hist = histogram_init(20, 0, 100, /* track_overflow */ true);

  TimerHandle_t load_timer = xTimerCreate(  // System load monitor
    "LOAD",
    LOAD_MONITOR_TASK_MS,
    pdTRUE, // uxAutoReload
    NULL,   // pvTimerID
    load_monitor_task_cb
  );

  xTimerStart(load_timer, 0);
#endif

  static PeriodicTaskCfg cfg = {  // LED blink handler
    .task = blink_task_cb,
    .ctx = NULL,
    .period = BLINK_TASK_MS,
    .repeat = REPEAT_FOREVER
  };

  create_periodic_task("BlinkLED", STACK_BYTES(256), TASK_PRIO_LOW, &cfg);


  static PeriodicTaskCfg console_task_cfg = { // Console command proc
    .task   = console_task_cb,
    .ctx    = NULL,
    .period = CONSOLE_TASK_MS,
    .repeat = REPEAT_FOREVER
  };

  create_periodic_task("Con", STACK_BYTES(4096), TASK_PRIO_LOW, &console_task_cfg);


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

}

