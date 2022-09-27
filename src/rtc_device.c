#include <stdbool.h>
#include <time.h>
#include <stdio.h>

#include "cstone/rtc_device.h"

static RTCDevice *s_sys_rtc = NULL;


RTCDevice *rtc_sys_device(void) {
  return s_sys_rtc;
}

void rtc_set_sys_device(RTCDevice *rtc) {
  s_sys_rtc = rtc;
}


void format_time(time_t t, char *buf, size_t buf_size) {
  struct tm lt;
  localtime_r(&t, &lt);

  snprintf(buf, buf_size, "%d-%02d-%02d %02d:%02d:%02d", lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday,
    lt.tm_hour, lt.tm_min , lt.tm_sec);
}
