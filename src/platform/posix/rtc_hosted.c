#include <stdbool.h>
#include <time.h>

#include "cstone/rtc_device.h"
#include "cstone/rtc_hosted.h"


static void rtc__hosted_set_time(RTCDevice *rtc, time_t time) {
}


static time_t rtc__hosted_get_time(RTCDevice *rtc) {
  return time(NULL);
}

static bool rtc__hosted_valid_time(RTCDevice *rtc) {
  return true;
}


bool rtc_hosted_init(RTCDevice *rtc) {
  rtc->set_time   = rtc__hosted_set_time;
  rtc->get_time   = rtc__hosted_get_time;
  rtc->valid_time = rtc__hosted_valid_time;
  rtc->calibrate  = NULL;

  return true;
}


