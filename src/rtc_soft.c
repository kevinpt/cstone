#define _XOPEN_SOURCE 700   // Needed for strptime()
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "lib_cfg/build_config.h"
#include "cstone/platform.h"
#if defined PLATFORM_HOSTED && !defined USE_ATOMIC_TIME
#  define  USE_ATOMIC_TIME
#endif

#if defined USE_ATOMIC_TIME
#  include <stdatomic.h>
#elif defined PLATFORM_EMBEDDED
#  include "cmsis_gcc.h"
#endif

#include "cstone/rtc_device.h"
#include "cstone/rtc_soft.h"

// https://stackoverflow.com/questions/71626597/what-are-the-various-ways-to-disable-and-re-enable-interrupts-in-stm32-microcont
#define ENTER_CRITICAL()  __disable_irq()
#define EXIT_CRITICAL()   __enable_irq()


// Context for RTCDevice
typedef struct {
#ifdef USE_ATOMIC_TIME
  // Note: this requires 64-bit atomic support via libatomic
  _Atomic time_t cur_time;
#else
  volatile time_t cur_time;
#endif
  bool is_valid;
} RTCSoft;



void rtc_soft_update(RTCDevice *rtc, int seconds) {
  RTCSoft *rtc_data = (RTCSoft *)rtc->ctx;
#ifdef USE_ATOMIC_TIME
  atomic_fetch_add(&rtc_data->cur_time, seconds);
#else
  ENTER_CRITICAL();
  rtc_data->cur_time += seconds;
  EXIT_CRITICAL();
#endif
}


static void rtc__soft_set_time(RTCDevice *rtc, time_t time) {
  RTCSoft *rtc_data = (RTCSoft *)rtc->ctx;
#ifdef USE_ATOMIC_TIME
  atomic_store(&rtc_data->cur_time, time);
#else
  ENTER_CRITICAL();
  rtc_data->cur_time = time;
  EXIT_CRITICAL();
#endif
  rtc_data->is_valid = true;
}


static time_t rtc__soft_get_time(RTCDevice *rtc) {
  RTCSoft *rtc_data = (RTCSoft *)rtc->ctx;
#ifdef USE_ATOMIC_TIME
  time_t time = atomic_load(&rtc_data->cur_time);
#else
  ENTER_CRITICAL();
  time_t time = rtc_data->cur_time;
  EXIT_CRITICAL();
#endif
  return time;
}


static bool rtc__soft_valid_time(RTCDevice *rtc) {
  RTCSoft *rtc_data = (RTCSoft *)rtc->ctx;

  return rtc_data->is_valid;
}


static bool rtc__soft_calibrate(RTCDevice *rtc, int cal_error, RTCCalibrateOp cal_op) {
  if(!(cal_op & RTC_CAL_DRY_RUN) && (cal_op & RTC_CAL_SET))
    rtc_soft_update(rtc, cal_error);
  return true;
}


bool rtc_soft_init(RTCDevice *rtc) {
  rtc->set_time   = rtc__soft_set_time;
  rtc->get_time   = rtc__soft_get_time;
  rtc->valid_time = rtc__soft_valid_time;
  rtc->calibrate  = rtc__soft_calibrate;

  RTCSoft *rtc_data = calloc(1, sizeof(RTCSoft));
  if(!rtc_data)
    return false;

  rtc->ctx = rtc_data;

  return true;
}







#ifdef TEST_RTC
#include <stdio.h>

static void time_to_str(struct tm *time, char *buf) {
  const char date_fmt[] = "%Y-%m-%d %H:%M:%S";
  strftime(buf, 64, date_fmt, time);
}

static void print_time(struct tm *time) {
  char buf[64];
  time_to_str(time, buf);
  printf("'%s' w:%d d:%d dst:%c", buf, time->tm_wday, time->tm_yday, time->tm_isdst ? 'D' : 's');
}

static void print_tm(struct tm *time) {
  printf("TM: %d-%d-%d %d:%d:%d %c", time->tm_year+1900, time->tm_mon+1, time->tm_mday,
          time->tm_hour, time->tm_min, time->tm_sec, time->tm_isdst ? 'D' : 's');
}


#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

int main(void) {
  typedef struct {
    const char *init_date;
    int inc_secs;
    const char *expect_date;
  } TestVec;

  const char date_fmt[] = "%Y-%m-%d %H:%M:%S";

  const TestVec test_vectors[] = {
    {"2021-12-31 23:59:59", 1, "2022-01-01 00:00:00"},
    {"2022-03-13 01:59:59", 1, "2022-03-13 03:00:00"},
    {"2022-04-13 01:59:59", 1, "2022-04-13 02:00:00"},
    {"2022-11-06 01:59:59", 1, "2022-11-06 01:00:00"}
  };

  char buf[64];
  RTCDevice rtc;
  rtc_soft_init(&rtc);
  rtc_set_sys_device(&rtc);

  for(int i = 0; i < COUNT_OF(test_vectors); i++) {
    const TestVec *tv = &test_vectors[i];

    struct tm init_time, rtc_time;
    puts("");

    strptime(tv->init_date, date_fmt, &init_time);
    init_time.tm_isdst = -1;
    time_t t = mktime(&init_time);
    // Let mktime() determine the DST status
    // Not that this fails for the ambiguous hour at the end of DST.
    // glibc treats the hour as DST if not specified as standard
    //init_time.tm_isdst = -1;
    rtc_set_time(rtc_sys_device(), t);
    rtc_soft_update(&rtc, tv->inc_secs);

    t = rtc_get_time(rtc_sys_device());
    localtime_r(&t, &rtc_time);
    strftime(buf, sizeof buf, date_fmt, &rtc_time);

    print_time(&init_time);
    fputs(" --> ", stdout);
    print_time(&rtc_time);
    
    printf("   Exp:'%s' %s\n", tv->expect_date, strcmp(tv->expect_date, buf) == 0 ? "match" : "BAD");
  }

  return 0;
}
#endif
