/* SPDX-License-Identifier: MIT
Copyright 2023 Kevin Thibedeau
(kevin 'period' thibedeau 'at' gmail 'punto' com)

See LICENSE for details
*/

/*
------------------------------------------------------------------------------
  Cron - Calendrical event scheduler

  This uses the RTC to control periodic event generation based on a schedule
  specification similar to that use in Unix crontab files.

  There is a single system wide list of cron entries that track schedule
  settings and their associated events. All event generation is synchronized
  to happen at the top of each minute as indicated by the RTC.

  Cron entries use events encoded as 32-bit prop IDs. They are sent via the
  system wide message hub to subscribed event listeners.

  Events can be generated in pairs where a second event represents an "off"
  condition that happens a specified number of minutes after the "on" event.
  This delay can be up to 24 hours (1440 minutes). This obviates the need to
  have multiple cron entries with coordinated schedules to manage on/off
  behaviors.

  Schedule fields can be a single value (n), range (n-n), or a wildcard (*).
  Ranges and wildcards can end with a step value (/n). Ranges are inclusive
  and cannot wrap around at 0 (e.g. "40-10/5" is invalid).

                         "* * * * *"
  Minute        (0-59) ---' | | | |
  Hour          (0-23) -----' | | |
  Day of month  (1-31) -------' | |
  Month         (1-12) ---------' |
  Day of week   (0-6)  -----------'


  Example schedules:
    Daily at midnight         "0 0 * * *"
    Hourly at half-past       "30 * * * *"
    Every 10 minutes          "0-59/10 * * * *"
    Every Monday at 2AM       "0 2 * * 1"
    M,W,F at noon             "0 12 * * 1-5/2"

  You cannot combine a specific day of month and day of week in the same
  schedule. The day of week will take precedence and day of month is ignored:

    Every Tuesday at midnight         "0 0 1 * 2"   ("day 1" is ignored)
    Every Tuesday at midnight         "0 0 * * 2"
    First of every month at midnight  "0 0 1 * *"


  Note that the month and day of month fields are 1-based in the specification
  string but they are 0-based everywhere else in the library such as struct
  CronTimeSpec. This differs from the behavior of struct tm where only the 
  day of month is 1-based.

------------------------------------------------------------------------------
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "util/list_ops.h"
#include "util/range_strings.h"
#include "util/mempool.h"
#include "util/getopt_r.h"
#include "util/term_color.h"
#include "util/string_ops.h"
#include "util/crc16.h"

#include "FreeRTOS.h"
#include "timers.h"
#include "semphr.h"

#include "cstone/rtos.h"
#include "cstone/blocking_io.h"
#include "cstone/prop_id.h"
#include "cstone/prop_db.h"
#include "cstone/umsg.h"
#include "cstone/cron_events.h"
#include "cstone/debug.h"

extern PropDB g_prop_db;

static CronEntry *s_cron_entry_list = NULL;
static SemaphoreHandle_t s_cron_lock = NULL;

#define LOCK()    xSemaphoreTake(s_cron_lock, portMAX_DELAY)
#define UNLOCK()  xSemaphoreGive(s_cron_lock)

#define MEM_CHECK(obj, size)  do { if(!(obj)) report_error(P_ERROR_SYS_MEM_ACCESS, (size)); } while(0)


static void *pool_alloc(size_t size, size_t *alloc_size) {
  void *obj = mp_alloc(mp_sys_pools(), size, alloc_size);
  MEM_CHECK(obj, size);

  return obj;
}


static uint64_t field_spec_to_bitmap(CronField *field) {
  uint64_t bitmap = 0;
  uint8_t step = field->step == 0 ? 1 : field->step; // 0 treated same as a step of 1

  if(field->rng_start == WILDCARD_START && step == 1) { // All bits active
    bitmap = 0xFFFFFFFFFFFFFFFFul;

  } else { // Populate selected bits covering the field range
    // Wildcard with a step >= 2 covers full range of minutes. Will be truncated by caller
    // for smaller fields.
    uint8_t rng_pos = field->rng_start == WILDCARD_START ? 0 : field->rng_start;
    uint8_t rng_end = field->rng_start == WILDCARD_START ? 59 : field->rng_end;
    do {
      bitmap |= 1ul << rng_pos;
      rng_pos += step;
    } while(rng_pos <= rng_end);
  }

  return bitmap;
}


static void map_init(CronMap *map, CronTimeSpec *spec) {
  map->minutes  = field_spec_to_bitmap(&spec->minute);
  map->hours    = (uint32_t)field_spec_to_bitmap(&spec->hour);
  map->days     = (uint32_t)field_spec_to_bitmap(&spec->day_of_month);
  map->months   = (uint16_t)field_spec_to_bitmap(&spec->month);
  map->days_of_week = (uint16_t)field_spec_to_bitmap(&spec->day_of_week);


  /*
    Can't allow days and days_of_weeks bitmaps to both be set.
    Give priority to days_of_week when it isn't wildcarded:

                    Days
                    0     Wild    Sparse
                 +-----------------------
    DOW   0      |  -     -       -
          Wild   |  -     -       DOW=0
          Sparse |  -     Days=0  Days=0
  */
  if(map->days_of_week != 0xFFFF && map->days_of_week != 0)
    map->days = 0;
  else if(map->days != 0)
    map->days_of_week = 0;
}


static bool prioritize_day_of_week(CronTimeSpec *spec) {
  if(spec->day_of_week.rng_start != WILDCARD_START)
    return true;
  else
    return spec->day_of_month.rng_start == WILDCARD_START;
}


static bool time_match(struct tm *now, CronMap *map) {
#define IS_SET(field, val)  (map->field & 1ul << (val))
  return IS_SET(minutes, now->tm_min) && IS_SET(hours, now->tm_hour) && IS_SET(months, now->tm_mon)
    && (IS_SET(days, now->tm_mday-1) || IS_SET(days_of_week, now->tm_wday));
}


static void do_pending_events(CronEntry *entries, time_t from, time_t until) {
  // Align to nearest minute after from
  time_t now = from + 60 - (from % 60);

  // Check for events on every minute within the pending range
  while(now <= until) {
    struct tm now_tm;
    localtime_r(&now, &now_tm);

    //DPRINT("%ld - %ld  now = %ld", from, until, now);

    CronEntry *cur = entries;
    while(cur) {
      bool remove_entry = false;

      if(time_match(&now_tm, &cur->active_map)) {
        if((cur->def.flags & CE_EVENT_STARTED) == 0) {
          // Send start event
          UMsg msg = { .id = cur->def.event,
            .source = P_RSRC_SYS_CRON_TASK };
          umsg_hub_send(umsg_sys_hub(), &msg, NO_TIMEOUT);


          if(cur->def.event_minutes > 0) {
            // Set map for end duration

            // Wildcard everything above an hour
            cur->active_map.days = 0xFFFFFFFF;
            cur->active_map.months = 0xFFFF;
            cur->active_map.days_of_week = 0xFFFF;

            // Add event_minutes offset to current time
            uint8_t hour = now_tm.tm_hour;
            uint8_t minute = now_tm.tm_min;
            hour += cur->def.event_minutes / 60;
            minute += cur->def.event_minutes % 60;
            if(minute > 59) { hour++; minute -= 60; }
            hour = hour % 24;
            cur->active_map.hours = 1ul << hour;
            cur->active_map.minutes = 1ul << minute;

            cur->def.flags |= CE_EVENT_STARTED;
          } else if(cur->def.flags & CE_ONE_SHOT) {
            remove_entry = true;
          }

        } else {
          // Send end event
          UMsg msg = { .id = cur->def.event_end,
            .source = P_RSRC_SYS_CRON_TASK };
          umsg_hub_send(umsg_sys_hub(), &msg, NO_TIMEOUT);


          // Restore original map
          map_init(&cur->active_map, &cur->def.spec);
          cur->def.flags &= ~CE_EVENT_STARTED;

          if(cur->def.flags & CE_ONE_SHOT)
            remove_entry = true;
        }
      }


      CronEntry *next = cur->next;
      if(remove_entry) {
        LOCK();
        ll_slist_remove(&s_cron_entry_list, cur);
        UNLOCK();
        mp_free(mp_sys_pools(), cur);
      }
      cur = next;
    }

    now += 60;  // Next minute in pending time span
  }
}

#define CRON_PERIOD_SECS  60  

static void rtos_sleep(TickType_t *prev_wake, time_t sleep_secs) {
  TickType_t now = xTaskGetTickCount();
  // Correct wake time if we've been suspended for more than the task period
  if((now - *prev_wake) >= pdMS_TO_TICKS(CRON_PERIOD_SECS * 1000))
    *prev_wake = now;

  TickType_t period_ticks = pdMS_TO_TICKS(sleep_secs * 1000);
  vTaskDelayUntil(prev_wake, period_ticks);
}


// TASK: Event scheduler
static void cron_task(void *ctx) {
  time_t from;
  time_t until = time(NULL);
  TickType_t prev_wake = xTaskGetTickCount();

  while(1) {
    from = until;

    // Sleep until top of next minute. This keeps us roughly synchronized with the RTC
    // regardless of any inaccuracies from the OS task scheduler.
    unsigned secs_elapsed = time(NULL) % CRON_PERIOD_SECS;
    rtos_sleep(&prev_wake, (time_t)CRON_PERIOD_SECS - secs_elapsed);

    until = time(NULL);

    // Detect +/- shifts in RTC
    int64_t delta = (int64_t)until - (int64_t)from;

    /* Scenarios involving time shifts:
      Move ahead 1 hour (start of DST)      Generate all events from the skipped hour
      Move ahead > 1 hour (anomaly)         Ignore
      Move ahead small amount (normal)      Generate all skipped events

      Move back 1 hour (end of DST)         Skip repeated events
      Move back > 1 hour (anomaly, epoch)   Ignore
      Move back small amount (adjust RTC)   Skip repeated events
    */
    if(delta > 0 && delta <= 3600 + CRON_PERIOD_SECS) {  // Time advanced normally
      LOCK();
      do_pending_events(s_cron_entry_list, from, until);
      UNLOCK();
    } else if(delta < CRON_PERIOD_SECS && delta >= -3600 - CRON_PERIOD_SECS) {
      // Sleep to avoid repeated time span
      rtos_sleep(&prev_wake, -delta - CRON_PERIOD_SECS);
    }

  }
}


void cron_init(void) {
  if(!s_cron_lock) {
    s_cron_lock = xSemaphoreCreateBinary();
    xSemaphoreGive(s_cron_lock);
  }

  xTaskCreate(cron_task, "cron", STACK_BYTES(1024), NULL, TASK_PRIO_LOW, NULL);
  cron_load_from_prop_db(&g_prop_db);
}


static CronEntry *cron__add_event_def(CronEntry **cron_list, CronDef *def, bool db_update) {
  CronEntry *entry = pool_alloc(sizeof *entry, NULL);
  if(!entry)
    return NULL;

  //DPRINT("entry size = %zu", sizeof *entry);

  memset(entry, 0, sizeof *entry);

  memcpy(&entry->def, def, sizeof *def);
  map_init(&entry->active_map, &def->spec);

  uint8_t clean_flags = def->flags;
  clean_flags &= CE_USER_FLAG_MASK;
  if(clean_flags & CE_ONE_SHOT) // CE_ONE_SHOT can't be persisted
    clean_flags &= ~CE_PERSIST;
  entry->def.flags = clean_flags;

  LOCK();
  ll_slist_push(cron_list, entry);
  UNLOCK();

  if(db_update && (clean_flags & CE_PERSIST))
    cron_save_to_prop_db(&g_prop_db);

  return entry;
}


bool cron_add_event_by_schedule(const char *schedule, uint32_t event, uint8_t flags,
                      uint32_t event_end, short event_minutes) {

  CronDef new_def = {
    .event = event,
    .event_end = event_end,
    .event_minutes = event_minutes,
    .flags = flags
  };

  if(!cron_decode_schedule(schedule, &new_def.spec))
    return false;

  CronEntry *entry = cron__add_event_def(&s_cron_entry_list, &new_def, /*db_update*/true);

  return entry != NULL;
}


bool cron_add_event_at_time(time_t at_time, uint32_t event) {
  struct tm at_tm;
  localtime_r(&at_time, &at_tm);

  CronDef new_def = {
    .spec = {
      .minute = {at_tm.tm_min, at_tm.tm_min, 0},
      .hour = {at_tm.tm_hour, at_tm.tm_hour, 0},
      .day_of_month = {at_tm.tm_mday-1, at_tm.tm_mday-1, 0},
      .month = {at_tm.tm_mon, at_tm.tm_mon, 0},
      .day_of_week = ANY_TIME
    },
    .event = event,
    .flags = CE_ONE_SHOT
  };

  CronEntry *entry = cron__add_event_def(&s_cron_entry_list, &new_def, /*db_update*/true);

  return entry != NULL;
}


bool cron_add_event(CronTimeSpec *spec, uint32_t event, uint8_t flags,
                      uint32_t event_end, short event_minutes) {

  CronDef new_def = {
    .spec   = *spec,
    .event  = event,
    .event_end      = event_end,
    .event_minutes  = event_minutes,
    .flags  = flags
  };

  CronEntry *entry = cron__add_event_def(&s_cron_entry_list, &new_def, /*db_update*/true);

  return entry != NULL;
}


static CronEntry *cron__event_find(uint32_t event) {
  CronEntry *rval = NULL;
  LOCK();
  for(CronEntry *cur = s_cron_entry_list; cur; cur = cur->next) {
    if(cur->def.event == event) {
      rval = cur;
      break;
    }
  }
  UNLOCK();
  return rval;
}


bool cron_remove_event(uint32_t event) {
  CronEntry *entry = cron__event_find(event);
  if(!entry)
    return false;

  LOCK();
  ll_slist_remove(&s_cron_entry_list, entry);
  UNLOCK();


  if(entry->def.flags & CE_PERSIST)
    cron_save_to_prop_db(&g_prop_db);

  mp_free(mp_sys_pools(), entry);
  return true;
}


static int encode_field(CronField *field, AppendRange *rng, int offset, bool last_field) {
  int size = 0;
  int status;

  if(field->rng_start == WILDCARD_START) {
    status = range_cat_char(rng, '*');
    size += (status < 0) ? -status : status;
  } else {
    status = range_cat_fmt(rng, "%d", field->rng_start + offset);
    size += (status < 0) ? -status : status;

    if(field->rng_end > field->rng_start) {
      status = range_cat_fmt(rng, "-%d", field->rng_end + offset);
      size += (status < 0) ? -status : status;
    }
  }

  if(field->step > 1) {
    status = range_cat_fmt(rng, "/%d", field->step);
    size += (status < 0) ? -status : status;
  }

  if(!last_field) {
    status = range_cat_char(rng, ' ');
    size += (status < 0) ? -status : status;
  }

  return size;
}


int cron_encode_schedule(CronTimeSpec *spec, AppendRange *rng) {
  int size = encode_field(&spec->minute, rng, 0, false);
  size += encode_field(&spec->hour, rng, 0, false);
  size += encode_field(&spec->day_of_month, rng, 1, false);
  size += encode_field(&spec->month, rng, 1, false);
  size += encode_field(&spec->day_of_week, rng, 0, true);

  return size;
}



static bool decode_field(const char *pos, const char **new_pos, CronField *field, int offset) {
  bool rval = true;
  char *npos;

  memset(field, 0, sizeof *field);

  // Skip leading whitespace
  while(*pos && isspace(*pos))
    pos++;

  int num_count = 0;

  while(num_count < 2 && *pos) {
    if(isspace(*pos)) // End of this field
      break;

    if(*pos == '*') { // Wildcard range
      pos++;
      field->rng_start = WILDCARD_START;
      field->rng_end = 0;
      num_count = 2;
      break;

    } else if(*pos == '-' && num_count == 1) { // End of range
      pos++;
      field->rng_end = strtoul(pos, &npos, 10) - offset;
      pos = (const char *)npos;
      num_count++;
      break;

    } else { // Start of range
      field->rng_start = strtoul(pos, &npos, 10) - offset;
      if(npos > pos) {
        pos = (const char *)npos;
        num_count++;
      } else { // Not a valid field
        rval = false;
        break;
      }

    }
  }

  // "*/n" or "s-e/n"
  if(*pos == '/') {
    if(num_count == 2) {
      pos++;
      field->step = strtoul(pos, &npos, 10);
      pos = (const char *)npos;
    } else {
      rval = false;
    }
  }

  *new_pos = pos;
  return rval;
}


bool cron_decode_schedule(const char *encoded, CronTimeSpec *spec) {
  const char *pos = encoded;

  if(!decode_field(pos, &pos, &spec->minute, 0)) return false;
  if(!decode_field(pos, &pos, &spec->hour, 0)) return false;
  if(!decode_field(pos, &pos, &spec->day_of_month, 1)) return false;
  if(!decode_field(pos, &pos, &spec->month, 1)) return false;
  if(!decode_field(pos, &pos, &spec->day_of_week, 0)) return false;

  return true;
}



static size_t cron__serialize(PropDB *db, CronSerialData **serial_data) {
  // Get count of persisted cron entries
  uint16_t count = 0;
  LOCK();
  for(CronEntry *cur = s_cron_entry_list; cur; cur = cur->next) {
    if(cur->def.flags & CE_PERSIST)
      count++;
  }
  UNLOCK();

  if(count == 0)
    return 0;

  size_t obj_size = sizeof(CronSerialData) + count*sizeof(CronDef);
  CronSerialData *serial = prop_db_alloc(db, obj_size, NULL);
  MEM_CHECK(serial, obj_size);
  if(!serial)
    return 0;
  memset(serial, 0, obj_size);

  // Serialize into array
  serial->count = count;
  LOCK();
  for(CronEntry *cur = s_cron_entry_list; cur; cur = cur->next) {
    if(count == 0) break;
    if(cur->def.flags & CE_PERSIST) {
      count--;
      memcpy(&serial->defs[count], &cur->def, sizeof(CronDef));
    }
  }
  UNLOCK();
  
  uint16_t crc = crc16_init();
  crc = crc16_update_small_block(crc, (uint8_t *)serial->defs, serial->count * sizeof(CronDef));
  serial->crc = crc16_finish(crc);

  *serial_data = serial;
  return obj_size;
}


bool cron_save_to_prop_db(PropDB *db) {
  CronSerialData *serial_data;
  size_t serial_size = cron__serialize(db, &serial_data);
  if(serial_size == 0) {  // No persistent cron entries left
    prop_del(db, P_SYS_CRON_LOCAL_VALUE);
    return true;
  }

  PropDBEntry db_entry = {
    .value      = (uintptr_t)serial_data,
    .size       = serial_size,
    .kind       = P_KIND_BLOB,
    .persist    = 1,
    .protect    = 1
  };
  bool rval = prop_set(db, P_SYS_CRON_LOCAL_VALUE, &db_entry, 0);
  if(!rval)
    prop_db_dealloc(db, serial_data);

  return rval;
}


static void print_cron_def(CronDef *def, bool verbose) {
  char buf[50];
  AppendRange rng;

  if(def->flags & CE_PERSIST)
    fputs(A_CYN, stdout);

  // Schedule
  range_init(&rng, buf, sizeof buf);
  cron_encode_schedule(&def->spec, &rng);
  bprintf("  %-20s", buf);

  // Flags
  range_init(&rng, buf, sizeof buf);
  range_cat_char(&rng, (def->flags & CE_PERSIST)? 'P' : '.');
  range_cat_char(&rng, (def->flags & CE_PROTECT)? 'S' : '.');
  range_cat_char(&rng, (def->flags & CE_ONE_SHOT)? 'O' : '.');
  printf("  %-5s", buf);

  // Event
  prop_get_name(def->event, buf, sizeof buf);
  printf("  %-20s", buf);

  // End event
  if(def->event_minutes > 0) {
    prop_get_name(def->event_end, buf, sizeof buf);
    printf("  %-20s  %d", buf, def->event_minutes);
  }

  puts(A_NONE);

  if(verbose) {
    range_init_empty(&rng, buf, sizeof buf);
    cron_describe_week_day(&def->spec, &rng);
    if(buf[0] != '\0') {
      bprintf("    %s\n", buf);
    } else {  // Day of month has priority
      range_init(&rng, buf, sizeof buf);
      cron_describe_month_day(&def->spec, &rng);
      bprintf("    %s\n", buf);
    }

    range_init(&rng, buf, sizeof buf);
    cron_describe_month(&def->spec, &rng);
    bprintf("    %s\n", buf);

    range_init(&rng, buf, sizeof buf);
    cron_describe_hour(&def->spec, &rng);
    bprintf("    %s\n", buf);

    range_init(&rng, buf, sizeof buf);
    cron_describe_minute(&def->spec, &rng);
    bprintf("    %s\n\n", buf);
  }
}


static bool cron__deserialize(CronSerialData *serial_data) {
  // Validate CRC
  uint16_t crc = crc16_init();
  crc = crc16_update_small_block(crc, (uint8_t *)serial_data->defs, serial_data->count * sizeof(CronDef));
  crc = crc16_finish(crc);
  //DPRINT("CRC (%d, %zu): %04X  %04X", serial_data->count, serial_data->count * sizeof(CronDef), serial_data->crc, crc);
  if(crc != serial_data->crc) {
    report_error((P1_ERROR | P2_CRON | P3_PROP | P4_INVALID), crc);
    return false;
  }


  // Remove all existing persistent cron entries
  LOCK();
  CronEntry *prev = NULL;
  CronEntry *next;
  for(CronEntry *cur = s_cron_entry_list; cur; cur = next) {
    if(cur->def.flags & CE_PERSIST) {
      next = cur->next;
      print_cron_def(&cur->def, /*verbose*/ false);
      if(prev)
        ll_slist_remove_after(prev);
      else
        ll_slist_pop(&s_cron_entry_list);

      mp_free(mp_sys_pools(), cur);

    } else { // Keep non-persistent entries
      prev = cur;
      next = cur->next;
    }
  }
  UNLOCK();

  // Load new entries
  for(int i = 0; i < serial_data->count; i++) {
    //print_cron_def(&serial_data->defs[i], /*verbose*/ false);
    cron__add_event_def(&s_cron_entry_list, &serial_data->defs[i], /*db_update*/false);
  }

  return true;
}

bool cron_load_from_prop_db(PropDB *db) {
  PropDBEntry entry;
  if(!prop_get(db, P_SYS_CRON_LOCAL_VALUE, &entry))
    return false;

  return cron__deserialize((CronSerialData *)entry.value);
}


typedef void (FormatItem)(AppendRange *rng, int value);



static void cron__describe_field(CronField *field, int max_value, FormatItem fmt_cb, AppendRange *rng) {
  int step = field->step > 0 ? field->step : 1;
  int rng_start = field->rng_start;
  int rng_end = field->rng_end > 0 ? field->rng_end : rng_start;

  if(rng_start == WILDCARD_START) {
    rng_start = 0;
    rng_end = max_value;
  }

  if(step <= 1 && rng_end > rng_start) { // Range summary
    fmt_cb(rng, rng_start);
    range_cat_str(rng, " to ");
    fmt_cb(rng, rng_end);

  } else { // Itemized list
    // Determine total item count to decide whether commas are needed
    unsigned count = 0;
    for(int i = rng_start; i <= rng_end && i <= max_value; i += step) {
      count++;
    }

    for(int i = rng_start; i <= rng_end && i <= max_value; i += step) {
      fmt_cb(rng, i);

      // Add comma between 3 or more items with a final conjunction
      if(i + step <= rng_end) { // Not last
        if(count > 2)
          range_cat_str(rng, ", ");
        else // No need for comma
          range_cat_char(rng, ' ');

        if(i + 2*step > rng_end) // Penultimate item followed by conjunction
          range_cat_str(rng, "and ");
      }
    }
  }
}


static void format_item_month(AppendRange *rng, int value) {
  static const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
    "Sep", "Oct", "Nov", "Dec"
  };
  range_cat_str(rng, month_names[value]);
}

void cron_describe_month(CronTimeSpec *spec, AppendRange *rng) {
  if(spec->month.rng_start == WILDCARD_START && spec->month.step <= 1) {
    range_cat_str(rng, "every month");
  } else {
    bool itemized = (spec->month.step > 1) || (spec->month.rng_end == 0);
    range_cat_str(rng, itemized ? "in " : "from ");
    cron__describe_field(&spec->month, 11, format_item_month, rng);
  }
}


static void format_item_week_day(AppendRange *rng, int value) {
  static const char *day_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
  range_cat_str(rng, day_names[value]);
}

void cron_describe_week_day(CronTimeSpec *spec, AppendRange *rng) {
  if(!prioritize_day_of_week(spec)) {
    range_cat_str(rng, "");
  } else {
    if(spec->day_of_week.rng_start == WILDCARD_START && spec->day_of_week.step <= 1) {
      range_cat_str(rng, "every weekday");
    } else {
      bool itemized = (spec->day_of_week.step > 1) || (spec->day_of_week.rng_end == 0);
      range_cat_str(rng, itemized ? "every week on " : "every week from ");
      cron__describe_field(&spec->day_of_week, 6, format_item_week_day, rng);
    }
  }
}


static void format_item_month_day(AppendRange *rng, int value) {
  range_cat_fmt(rng, "%d", value+1);
}

void cron_describe_month_day(CronTimeSpec *spec, AppendRange *rng) {
  if(prioritize_day_of_week(spec)) {
    range_cat_str(rng, "");
  } else {
    if(spec->day_of_month.rng_start == WILDCARD_START && spec->day_of_month.step <= 1) {
      range_cat_str(rng, "every day");
    } else {
      bool itemized = (spec->day_of_month.step > 1) || (spec->day_of_month.rng_end == 0);
      range_cat_str(rng, itemized ? "on date " : "from day ");
      cron__describe_field(&spec->day_of_month, 30, format_item_month_day, rng);
    }
  }
}


static void format_item_hour(AppendRange *rng, int value) {
  range_cat_fmt(rng, "%d%c", value > 12 ? value-12 : value==0 ? 12 : value, value < 12 ? 'A' : 'P');
}

void cron_describe_hour(CronTimeSpec *spec, AppendRange *rng) {
  if(spec->hour.rng_start == WILDCARD_START && spec->hour.step <= 1) {
    range_cat_str(rng, "every hour");
  } else {
    bool itemized = (spec->hour.step > 1) || (spec->hour.rng_end == 0);
    range_cat_str(rng, itemized ? "at " : "from ");
    cron__describe_field(&spec->hour, 23, format_item_hour, rng);
  }
}


static void format_item_minute(AppendRange *rng, int value) {
  range_cat_fmt(rng, "%d", value);
}

void cron_describe_minute(CronTimeSpec *spec, AppendRange *rng) {
  if(spec->minute.rng_start == WILDCARD_START && spec->minute.step <= 1) {
    range_cat_str(rng, "every minute");
  } else {
    bool itemized = (spec->minute.step > 1) || (spec->minute.rng_end == 0);
    range_cat_str(rng, itemized ? "at " : "from ");
    cron__describe_field(&spec->minute, 59, format_item_minute, rng);
    range_cat_str(rng, " minutes past");
  }
}


static void hline(const char *line, unsigned count, unsigned indent) {
  printf("%*s", indent, " ");

  while(count--) {
    fputs(line, stdout);
  }
  puts(A_NONE);
}





int32_t cmd_cron(uint8_t argc, char *argv[], void *eval_ctx) {
  GetoptState state = {0};
  state.report_errors = true;
  int c;

  char *add_sched = NULL;
  char *add_event = NULL;
  char *del_entry = NULL;
  uint8_t flags = 0;
  bool load_prop = false;
  bool verbose = false;

  while((c = getopt_r(argv, "a:e:d:polvh", &state)) != -1) {
    switch(c) {
    case 'a': add_sched = (char *)state.optarg; break;
    case 'e': add_event = (char *)state.optarg; break;
    case 'd': del_entry = (char *)state.optarg; break;
    case 'p': flags |= CE_PERSIST; break;
    case 'o': flags |= CE_ONE_SHOT; break;
    case 'l': load_prop = true; break;
    case 'v': verbose = true; break;

    case 'h':
      puts("cron [-a <sched> -e <event>] [-p] [-o] [-d <event>] [-l] [-v] [-h]");
      puts( "  Schedule:  \"Min Hr Day Mon DoW\"\n");
      puts( "  Field values:\n"
            "     n:  Once at specific time\n"
            "     *:  All times");
      puts( "   s-e:  Repeat every time from (s)tart to (e)nd\n"
            " s-e/n:  Repeat every n-th time from (s)tart to (e)nd\n"
            "   */n:  Repeat every n-th time\n");
      puts( "  Event: \"<start>[, <end>]\"");
      return 0;
      break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }
  
  if(load_prop) {
    return cron_load_from_prop_db(&g_prop_db) ? 0 : -5;
  }

  if(add_sched) {
    if(!add_event) {
      puts("Missing required event");
      return -4;
    }

    CronTimeSpec new_sched;
    if(!cron_decode_schedule(add_sched, &new_sched)) {
      puts("Invalid schedule");
      return -4;
    }

    // Split event arg
    char *event_fields[3];
    int field_count = str_split(add_event, ", ", event_fields, COUNT_OF(event_fields));
    if(!(field_count == 1 || field_count == 3)) {
      puts("Invalid event specification");
      return -4;
    }
    uint32_t event;
    uint32_t end_event = 0;
    unsigned event_minutes = 0;

    event = prop_parse_any(event_fields[0]);
    if(event == 0) {
      printf("Unrecognized event: %s\n", add_event);
      return -4;
    }

    if(field_count == 3) {
      end_event = prop_parse_any(event_fields[1]);
      event_minutes = strtoul(event_fields[2], NULL, 10);
    }

    bool status = cron_add_event(&new_sched, event, flags, end_event, event_minutes);
    printf("%s\n", status ? "Success" : "Failed");

  } else if(del_entry) {
    uint32_t event = prop_parse_any(del_entry);
    if(event == 0) {
      printf("Unrecognized event: %s\n", del_entry);
      return -4;
    }

    if(cron_remove_event(event))
      puts("Success");

  } else {  // Print summary
    if(!s_cron_entry_list) {
      puts("  No cron entries");

    } else {
      puts(A_YLW "     Schedule          Flags      Event                 End event      Duration");
      hline(u8"─", 77, 2);
      LOCK();
      for(CronEntry *cur = s_cron_entry_list; cur; cur = cur->next) {
        print_cron_def(&cur->def, verbose);
      }
      UNLOCK();
    }
  }

  return 0;
}

