#ifndef CRON_EVENT_H
#define CRON_EVENT_H

#define P_RSRC_SYS_CRON_TASK          (P1_RSRC | P2_SYS | P3_CRON | P4_TASK)
#define P_EVENT_SYS_CRON_UPDATE       (P1_EVENT | P2_SYS | P3_CRON | P4_UPDATE)

typedef struct {
  uint16_t rng_start :  6;  // 63 for wildcard
  uint16_t rng_end   :  6;
  uint16_t step      :  4;  // 0 treated same as a step of 1
} CronField;

// Specification for cron times used to generate CronMap in entry object
typedef struct {
  CronField minute;       // 0-59
  CronField hour;         // 0-23
  CronField day_of_month; // 0-30
  CronField month;        // 0-11
  CronField day_of_week;  // 0-6
} CronTimeSpec;


typedef struct {
  uint32_t  event;
  uint32_t  event_end;
  CronTimeSpec spec;
  short     event_minutes;  // Optional duration until event_end is sent; 0 for no end event*/
  uint8_t   flags;
} CronDef;


typedef struct {
  // Bitmaps for each element of broken down time 
  uint64_t minutes;
  uint32_t days;
  uint32_t hours;
  uint16_t months;
  uint16_t days_of_week;
} CronMap;


typedef struct {
  uint16_t  count;
  uint16_t  crc;  // Covers defs array
  CronDef   defs[];
} CronSerialData;

// Flags for CronEntry
#define CE_PERSIST        0x01
#define CE_PROTECT        0x02
#define CE_ONE_SHOT       0x04

#define CE_USER_FLAG_MASK 0x7F

// Internal flags
#define CE_EVENT_STARTED  0x80

typedef struct CronEntry {
  struct CronEntry *next;
  CronDef def;
  CronMap active_map;
} CronEntry;

// Wildcard value for CronTimeSpec fields
#define WILDCARD_START  63
#define ANY_TIME  {WILDCARD_START,0,0}

#define CE_SUN 0
#define CE_MON 1
#define CE_TUE 2
#define CE_WED 3
#define CE_THU 4
#define CE_FRI 5
#define CE_SAT 6

#define CE_JAN 0
#define CE_FEB 1
#define CE_MAR 2
#define CE_APR 3
#define CE_MAY 4
#define CE_JUN 5
#define CE_JUL 6
#define CE_AUG 7
#define CE_SEP 8
#define CE_OCT 9
#define CE_NOV 10
#define CE_DEC 11

#define CE_YEARLY  (CronTimeSpec){ .day_of_month = {0,0,1}, .month = {0,0,1}, .day_of_week = ANY_TIME }
#define CE_MONTHLY (CronTimeSpec){ .day_of_month = {0,0,1}, .month = ANY_TIME, .day_of_week = ANY_TIME }
#define CE_DAILY   (CronTimeSpec){ .day_of_month = ANY_TIME, .month = ANY_TIME, .day_of_week = ANY_TIME }
#define CE_WEEKLY  (CronTimeSpec){ .day_of_month = ANY_TIME, .month = ANY_TIME, .day_of_week = {0,0,1} }
#define CE_HOURLY  (CronTimeSpec){ .hour = ANY_TIME, .day_of_month = ANY_TIME, .month = ANY_TIME, \
                                      .day_of_week = ANY_TIME }

#ifdef __cplusplus
extern "C" {
#endif

void cron_init(void);

// ******************** Event control ********************
//bool cron_event_add(CronTimeSpec *spec, uint32_t event, uint8_t flags);
bool cron_add_event(CronTimeSpec *spec, uint32_t event, uint8_t flags,
                      uint32_t event_end, short event_minutes);
bool cron_add_event_by_schedule(const char *schedule, uint32_t event, uint8_t flags,
                      uint32_t event_end, short event_minutes);
bool cron_add_event_at_time(time_t at_time, uint32_t event);
bool cron_remove_event(uint32_t event);

// ******************** Utility ********************
int cron_encode_schedule(CronTimeSpec *spec, AppendRange *rng);
bool cron_decode_schedule(const char *encoded, CronTimeSpec *spec);

bool cron_save_to_prop_db(PropDB *db);
bool cron_load_from_prop_db(PropDB *db);

int32_t cmd_cron(uint8_t argc, char *argv[], void *eval_ctx);

#ifdef __cplusplus
}
#endif

#endif // CRON_EVENT_H
