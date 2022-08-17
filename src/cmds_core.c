#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "cstone/platform.h"
#include "build_config.h"
#include "build_info.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "cstone/console.h"

#include "cstone/prop_id.h"
#include "cstone/prop_db.h"
#include "cstone/debug.h"
#include "cstone/error_log.h"
#ifdef PLATFORM_EMBEDDED
#  include "cstone/faults.h"
#endif
#include "util/mempool.h"
#include "cstone/rtos.h"
#include "cstone/log_db.h"
#include "cstone/log_info.h"
#include "cstone/log_props.h"
#include "cstone/target.h"
#include "cstone/timing.h"
#include "cstone/tasks_core.h"
#include "cstone/blocking_io.h"

#include "util/getopt_r.h"
#include "util/string_ops.h"
#include "util/range_strings.h"
#include "bsd/string.h"


extern PropDB     g_prop_db;
extern ErrorLog   g_error_log;
extern mpPoolSet  g_pool_set;
extern LogDB      g_log_db;


static int32_t cmd_build(uint8_t argc, char *argv[], void *eval_ctx) {
  printf("  %s\n", g_build_date);
  printf("    Firmware: %s\n", APP_VERSION);
  printf("    FreeRTOS: %s\n", tskKERNEL_VERSION_NUMBER);
  return 0;
}


static int32_t cmd_clear(uint8_t argc, char *argv[], void *eval_ctx) {
  fputs("\033[H\033[2J", stdout);
  return 0;
}


static int32_t cmd_debug(uint8_t argc, char *argv[], void *eval_ctx) {
  // Configure global debug mode and provide control of debug feature flags
  GetoptState state = {0};
  state.report_errors = true;

  int c;
  bool list_flags = false;
  unsigned int debug_level = g_debug_level;
  char *field_arg = NULL;

  while((c = getopt_r(argv, "lf:nyvh", &state)) != -1) {
    switch(c) {
    case 'l':
      list_flags = true; break;

    case 'f':
      field_arg = (char *)state.optarg;
      break;

    case 'n':
      debug_level = DEBUG_LEVEL_NONE; break;

    case 'y':
      debug_level = DEBUG_LEVEL_BASIC; break;

    case 'v':
      debug_level = DEBUG_LEVEL_VERBOSE; break;

    case 'h':
      puts("Set debug level:  debug [-n] [-y] [-v]");
      puts("List flags:       debug -l");
      puts("Set flag:         debug -f <flag name>=<bool value>");
      return 0;
      break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }

  if(debug_level != g_debug_level) {  // New debug level
    prop_set_uint(&g_prop_db, (P1_DEBUG | P2_SYS | P3_LOCAL | P4_VALUE), debug_level,
                  (P1_RSRC | P2_CON | P3_LOCAL | P4_TASK));

    // NOTE: g_debug_level is updated in event monitor triggered by prop update
  }

  fputs("Debug level: ", stdout);
  switch(debug_level) {
    case DEBUG_LEVEL_NONE:  puts("off"); break;
    case DEBUG_LEVEL_BASIC:  puts("ON"); break;
    default:
    case DEBUG_LEVEL_VERBOSE:  puts("VERBOSE"); break;
  }

  if(!field_arg) {
    if(list_flags)
      debug_flag_dump();

  } else { // Modify a flag
    char *save_state;
    char *field_name = strtok_r(field_arg, "=", &save_state);
    char *field_value = NULL;
    if(field_name) {
      field_value = strtok_r(NULL, "=", &save_state);
    }

    if(field_value) {
      bool value = str_to_bool(field_value);
      if(debug_flag_set_by_name(field_name, value))
        printf("  '%s' = %c\n", field_name, value ? '1' : '0');
      else
        printf("  Unknown field '%s'\n", field_name);

    } else {
      puts("  Missing value");
    }
  }

  return 0;
}


static int32_t cmd_elog(uint8_t argc, char *argv[], void *eval_ctx) {
  // Manage error log
  GetoptState state = {0};
  state.report_errors = true;

  int c;
  bool read = false;
  bool clear = false;
  bool dump = false;
  bool mount = false;

  static const struct option long_options[] = {
    {"clear",  no_argument, NULL, 'c'},
    {"read",    no_argument, NULL, 'r'},
    {"dump",    no_argument, NULL, 'd'},
    {"mount",   no_argument, NULL, 'm'},
    {0}
  };

  while((c = getopt_long_r(argv, "crdm", long_options, &state)) != -1) {
    switch(c) {
    case 'c':
      clear = true; break;
    case 'r':
      read = true; break;
    case 'd':
      dump = true; break;
    case 'm':
      mount = true; break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }

  if(mount) {
    DPUTS("Mount elog");
    errlog_mount(&g_error_log); // FIXME: Mount creates 0.0.0.0 error
    return 0;

  } else if(clear) {
    errlog_format(&g_error_log);
    return 0;

  } else if(read) { // FIXME: Make this work again
    //errlog_dump_record(&g_error_log);
    return 0; // Can't dump while printing read data
  }


  errlog_print_all(&g_error_log);

  if(dump)
    errlog_dump_raw(&g_error_log, errlog_size(&g_error_log), 0);

  return 0;
}


static int32_t cmd_error(uint8_t argc, char *argv[], void *eval_ctx) {
  static unsigned error_count = 0;
#if 0
  GetoptState state = {0};
  state.report_errors = true;

  int c;
  bool do_fatal_error = false;

  while((c = getopt_r(argv, "fh", &state)) != -1) {
    switch(c) {
    case 'f':
      do_fatal_error = true;
      break;

    case 'h':
      puts("error [-f] [-h]");
      return 0;
      break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }

#endif
//  if(do_fatal_error) {
//    puts("Fatal error shutdown");
//    fatal_error();

//  } else {  // Send test error
    report_error(P1_ERROR | P2_CON | P3_LOCAL | P3_ARR(error_count), error_count);
    error_count++;
//  }

  return 0;
}


#ifndef PLATFORM_EMBEDDED
static int32_t cmd_exit(uint8_t argc, char *argv[], void *eval_ctx) {
  exit(0);
  return 0;
}
#endif


#ifdef PLATFORM_EMBEDDED
static int32_t cmd_fault(uint8_t argc, char *argv[], void *eval_ctx) {
  // Report any fault condition prior to reboot
  // Manually generate faults for testing
  GetoptState state = {0};
  state.report_errors = true;
  int c;

  enum FaultTrigger {
    FAULT_NONE = 0,
    FAULT_DIV0,
    FAULT_BAD_ADDR,
    FAULT_INSTR,
    FAULT_STACK
  };

  enum FaultTrigger trigger_fault = FAULT_NONE;

  while((c = getopt_r(argv, "aiszh", &state)) != -1) {
    switch(c) {
    case 'a': trigger_fault = FAULT_BAD_ADDR; break;
    case 'i': trigger_fault = FAULT_INSTR; break;
    case 's': trigger_fault = FAULT_STACK; break;
    case 'z': trigger_fault = FAULT_DIV0; break;

    case 'h':
      puts("fault [-a] [-i] [-s] [-z] [-h]");
      return 0;
      break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }

  switch(trigger_fault) {
  case FAULT_DIV0:     trigger_fault_div0(); break;
  case FAULT_BAD_ADDR: trigger_fault_bad_addr(); break;
  case FAULT_INSTR:    trigger_fault_illegal_instruction(); break;
  case FAULT_STACK:    trigger_fault_stack(); break;

  default:
    if(!report_faults(&g_prev_fault_record, /*verbose*/true))
      puts("  No faults");

    break;
  }

  return 0;
}
#endif


static int32_t cmd_free(uint8_t argc, char *argv[], void *eval_ctx) {
  GetoptState state = {0};
  state.report_errors = true;

  int c;
  bool show_hist = false;

  while((c = getopt_r(argv, "p", &state)) != -1) {
    switch(c) {
    case 'p':
      show_hist = true;
      break;

    default:
    case ':':
    case '?':
      return -2;
      break;
    }
  }

  if(!show_hist) {
    puts("FreeRTOS heap:");

    size_t heap_size = heap_os_size();
    size_t heap_free = heap_os_free();
    size_t heap_min_free = heap_os_min_free();

    printf("\tTotal:   %3" PRIuz " KB\n", heap_size / 1024);
    printf("\tUsed:    %3" PRIuz " KB", (heap_size - heap_free) / 1024);
    printf("\t\t\tObjects: %" PRIuz "\n",  heap_os_allocated_objs());
    printf("\tFree:    %3" PRIuz " KB (Min %" PRIuz " KB)\n", heap_free / 1024, heap_min_free / 1024);


    bputs("\nC heap:");

    heap_size = heap_c_lib_size();
    heap_free = heap_c_lib_free();

    printf("\tTotal:   %3" PRIuz " KB\n", heap_size / 1024);
    printf("\tUsed:    %3" PRIuz " KB\n", (heap_size - heap_free) / 1024);
    printf("\tFree:    %3" PRIuz " KB\n", heap_free / 1024);

    bputs("\n--= Memory pools =--");
    mp_summary(&g_pool_set);

  } else {  // Plot mem pool histogram
    puts("Memory pool requests:");
    mp_plot_stats(&g_pool_set);
  }

  return 0;
}


#ifdef USE_CONSOLE_HISTORY
static int32_t cmd_history(uint8_t argc, char *argv[], void *eval_ctx) {
  Console *con = active_console();
  if(con)
    shell_show_history(&con->shell);
  return 0;
}
#endif


static int32_t cmd_log(uint8_t argc, char *argv[], void *eval_ctx) {
  GetoptState state = {0};
  state.report_errors = true;

  int c;
  bool read = false;
  bool save = false;
  bool format = false;
  bool dump = false;

  static const struct option long_options[] = {
    {"format",  no_argument, NULL, 'f'},
    {"read",    no_argument, NULL, 'r'},
    {"save",    no_argument, NULL, 's'},
    {"dump",    no_argument, NULL, 'd'},
    {0}
  };

  while((c = getopt_long_r(argv, "frsd", long_options, &state)) != -1) {
    switch(c) {
    case 'f':
      format = true; break;
    case 'r':
      read = true; break;
    case 's':
      save = true; break;
    case 'd':
      dump = true; break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }

  if(format) {
    logdb_format(&g_log_db);

  } else if(read) {
    logdb_dump_record(&g_log_db);
    //restore_logged_props(&g_prop_db);
    return 0; // Can't dump while printing read data

  } else if(save) {
    puts("Save props...");
    save_props_to_log(&g_prop_db, &g_log_db, /*compress*/true);
  }

  if(dump)
    logdb_dump_raw(&g_log_db, 512, 0);

  return 0;
}



static int32_t cmd_property(uint8_t argc, char *argv[], void *eval_ctx) {
  GetoptState state = {0};
  state.report_errors = true;

  int c;
  char *prop_name  = NULL;
  const char *prop_value = NULL;
  bool list_all = (argc == 1);

  while((c = getopt_r(argv, "h", &state)) != -1) {
    switch(c) {
    case 'h':
      puts("List all properties:  PROPerty");
      puts("List named property:  PROPerty [name]");
      puts("Set property          PROPerty <name>=<value>");
      return 0;
      break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }

  if(list_all) {
#if 0
    prop_db_dump(&g_prop_db); // Unsorted dump

#else
    // Sorted dump
    uint32_t *keys;
    size_t keys_len = prop_db_all_keys(&g_prop_db, &keys);
    if(keys_len > 0) {
      prop_db_sort_keys(&g_prop_db, keys, keys_len);
      prop_db_dump_keys(&g_prop_db, keys, keys_len);
      cs_free(keys);
    }
#endif
    return 0;
  }


  if(state.optind < argc) { // Non-option args
    prop_name = argv[state.optind++];
  }

  if(!prop_name)
    return -1;

  // Check if this is an assignment
  char *eq_pos = strchr(prop_name, '=');
  if(eq_pos) {
    prop_value = eq_pos+1;
    *eq_pos = '\0';
  }

  // Check if this is an ID string (Pnnnnnnnn)
  uint32_t prop = prop_parse_id(prop_name);

  // Otherwise, check if it's a full name
  if(prop == 0)
    prop = prop_parse_name(prop_name);

  if(prop_value) {  // Assign new value to property
    printf("Setting prop " PROP_ID " = '%s'\n", prop, prop_value);

    char *pos;
    uint32_t int_value;
    bool bool_value;

    // Attempt to parse value as number
    if(prop_value[0] == '-')  // Signed value
      int_value = (uint32_t)strtol(prop_value, &pos, 0);
    else  // Unsigned value
      int_value = strtoul(prop_value, &pos, 0);

    if(pos > prop_value) {  // Parsed as int
      PropDBEntry new_value = {0};
      new_value.value = int_value;
      new_value.kind = P_KIND_UINT;
      prop_set(&g_prop_db, prop, &new_value, (P1_RSRC | P2_CON | P3_LOCAL | P4_TASK));

    } else if(str_is_bool(prop_value, &bool_value)) {  // Parse as boolean
      PropDBEntry new_value = {0};
      new_value.value = bool_value;
      new_value.kind = P_KIND_UINT;
      prop_set(&g_prop_db, prop, &new_value, (P1_RSRC | P2_CON | P3_LOCAL | P4_TASK));

    } else { // Treat as a string value
      size_t new_str_size;
      char *new_str = (char *)prop_db_alloc(&g_prop_db, strlen(prop_value)+1, &new_str_size);

      if(new_str) {
        //snprintf(new_str, new_str_size, "%s", prop_value);
        strlcpy(new_str, prop_value, new_str_size);
        prop_set_str(&g_prop_db, prop, new_str, (P1_RSRC | P2_CON | P3_LOCAL | P4_TASK));
      }
    }
  }

  if(prop == 0 || !prop_print(&g_prop_db, prop))
    puts("Invalid property");


  return 0;
}


#ifdef PLATFORM_EMBEDDED
static int32_t cmd_peek(uint8_t argc, char *argv[], void *eval_ctx) {
  if(argc != 2)
    return -1;

  uint32_t addr = strtoul(argv[1], NULL, 16);
  uint32_t reg = *(uint32_t *)addr;

  puts("Addr:                  " A_YLW "31       23       15       7" A_NONE);
  printf("  %08" PRIX32 " = %08" PRIX32 " " A_GRN "|" A_NONE, addr, reg);
  for(int pos = 31; pos >= 0; pos--) {
    putc((reg & (1ul << pos)) ? '1':'0', stdout);
    if(pos > 0 && pos % 8 == 0)
      putc(' ', stdout);
  }
  puts(A_GRN "|" A_NONE);

  return 0;
}
#endif


static int32_t cmd_reset(uint8_t argc, char *argv[], void *eval_ctx) {
  software_reset();
  return 0;
}


static void resize_verbose_cb(void *ctx) {
  Console *con = (Console *)ctx;

  char buf[10];
  AppendRange rng = RANGE_FROM_ARRAY(buf);
  range_cat_uint(&rng, con->term_size.cols);
  range_cat_char(&rng, 'x');
  range_cat_uint(&rng, con->term_size.rows);
  puts(buf);

  shell_suppress_prompt(&con->shell, false);
  shell_show_prompt(&con->shell);
}


static int32_t cmd_resize(uint8_t argc, char *argv[], void *eval_ctx) {
  GetoptState state = {0};
  state.report_errors = true;

  int c;
  bool verbose = false;

  while((c = getopt_r(argv, "v", &state)) != -1) {
    switch(c) {
    case 'v':
      verbose = true;
      break;

    default:
    case ':':
    case '?':
      return -2;
      break;
    }
  }

  Console *con = active_console();
  console_query_terminal_size(con);

  if(verbose) {
    // We need a delay to wait for query response. This is running in the same
    // task as shell_process_rx() so we can't block. Create a temporary task instead.
    create_delayed_task(resize_verbose_cb, con, STACK_BYTES(512), 80);
    shell_suppress_prompt(&con->shell, true);
  }

  return 0;
}

static unsigned long ticks_to_si_time(unsigned long ticks, const char **unit) {
  // FreeRTOS configured to use performance timer
  unsigned long usec = ticks * (1000000ul / PERF_CLOCK_HZ);

  if(usec < 1000) {
    *unit = "us";
    return usec;
  }

  unsigned long msec = ticks / (PERF_CLOCK_HZ / 1000ul);
  if(msec < 1000) {
    *unit = "ms";
    return msec;
  }

  if(msec < 60*1000) {
    *unit = "sec";
    return msec / 1000;
  }

  *unit = "min";
  return msec / 1000 / 60;
}



static int32_t cmd_tasks(uint8_t argc, char *argv[], void *eval_ctx) {
  UBaseType_t num_tasks = uxTaskGetNumberOfTasks();

  TaskStatus_t *tasks = (TaskStatus_t *)malloc(num_tasks * sizeof(TaskStatus_t));
  if(!tasks)
    return -1;

  uint32_t run_time;
  const char *unit;
  unsigned long task_time;

  num_tasks = uxTaskGetSystemState(tasks, num_tasks, &run_time);

  printf("%lu tasks:\n", num_tasks);
  puts(A_YLW "     Name    State  Prio  Stack  Run");
#if 0
  puts(      "  -------------------------------------" A_NONE);
#else
  puts(    u8"  ─────────────────────────────────────" A_NONE);
#endif

  for(UBaseType_t i = 0; i < num_tasks; i++) {
    char state = ' ';
    switch(tasks[i].eCurrentState) {
    case eReady:      state = 'r'; break;
    case eRunning:    state = 'R'; break;
    case eBlocked:    state = 'b'; break;
    case eSuspended:  state = 's'; break;
    case eDeleted:    state = 'd'; break;
    case eInvalid:    state = 'i'; break;
    }

    task_time = ticks_to_si_time(tasks[i].ulRunTimeCounter, &unit);
    printf("  %-10s   %c    %2lu    %4" PRIu32 "  %3lu %s\n", tasks[i].pcTaskName, state,
            tasks[i].uxCurrentPriority, (uint32_t)(tasks[i].usStackHighWaterMark * sizeof(StackType_t)),
            task_time, unit);
  }

  printf("\n  Avg. load: %" PRIu32 "%%\n", 100 - ulTaskGetIdleRunTimePercent());
  free(tasks);

#ifdef USE_LOAD_MONITOR
  plot_load_stats();
#endif

  return 0;
}


static int32_t cmd_uptime(uint8_t argc, char *argv[], void *eval_ctx) {
  unsigned long msec, sec, min, hour, day;
  msec = millis();

  sec = (msec + 500) / 1000;
  min = sec / 60;
  sec %= 60;
  hour = min / 60;
  min %= 60;
  day = hour / 24;
  hour %= 24;

  printf("  %lu ms  %u days %02u:%02u:%02u\n", msec, (uint16_t)day, (uint16_t)hour,
          (uint16_t)min, (uint16_t)sec);
  return 0;
}


const ConsoleCommandDef g_core_cmd_set[] = {
  CMD_DEF("build",    cmd_build,      "Report build info"),
  CMD_DEF("clear",    cmd_clear,      "Clear screen"),
  CMD_DEF("debug",    cmd_debug,      "Config debug modes"),
  CMD_DEF("elog",     cmd_elog,       "Dump error log"),
  CMD_DEF("error",    cmd_error,      "Test error"),
#ifndef PLATFORM_EMBEDDED
  CMD_DEF("exit",     cmd_exit,       "Terminate shell"),
#endif
#ifdef PLATFORM_EMBEDDED
  CMD_DEF("fault",    cmd_fault,      "Trigger hard fault"),
#endif
  CMD_DEF("free",     cmd_free,       "Memory use"),
#ifdef USE_CONSOLE_HISTORY
  CMD_DEF("HISTory",  cmd_history,    "Cmd history"),
#endif
  CMD_DEF("log",      cmd_log,        "Dump log FS"),
  CMD_DEF("PROPerty", cmd_property,   "Get prop"),
#ifdef PLATFORM_EMBEDDED
  CMD_DEF("peek",     cmd_peek,       "Dump memory"),
#endif
  CMD_DEF("reset",    cmd_reset,      "Reset system"),
  CMD_DEF("resize",   cmd_resize,     "Get term size"),
  CMD_DEF("tasks",    cmd_tasks,      "List tasks"),
  CMD_DEF("UPtime",   cmd_uptime,     "Report uptime"),
  CMD_END
};
