#ifndef RTC_DEVICE_H
#define RTC_DEVICE_H


typedef enum {
  RTC_CAL_SET     = 0x01,
  RTC_CAL_CLEAR   = 0x02,
  RTC_CAL_DRY_RUN = 0x80
} RTCCalibrateOp;

typedef struct RTCDevice  RTCDevice;

// Device callbacks
typedef void (*RTCSetTime)(RTCDevice *rtc, time_t time);
typedef time_t (*RTCGetTime)(RTCDevice *rtc);
typedef bool (*RTCValidTime)(RTCDevice *rtc);
typedef bool (*RTCCalibrate)(RTCDevice *rtc, int cal_error, RTCCalibrateOp cal_op);

struct RTCDevice {
  RTCSetTime    set_time;
  RTCGetTime    get_time;
  RTCValidTime  valid_time;
  RTCCalibrate  calibrate;
  void *ctx;
};


#ifdef __cplusplus
extern "C" {
#endif

static inline void rtc_set_time(RTCDevice *rtc, time_t time) {
  if(rtc)
    rtc->set_time(rtc, time);
}


static inline time_t rtc_get_time(RTCDevice *rtc) {
  if(rtc)
    return rtc->get_time(rtc);
  else
    return 0;
}


static inline bool rtc_valid_time(RTCDevice *rtc) {
  if(rtc)
    return rtc->valid_time(rtc);

  return false;
}


static inline bool rtc_calibrate(RTCDevice *rtc, int cal_error, RTCCalibrateOp cal_op) {
  if(!rtc || !rtc->calibrate)
    return false;

  return rtc->calibrate(rtc, cal_error, cal_op);
}


RTCDevice *rtc_sys_device(void);
void rtc_set_sys_device(RTCDevice *rtc);
void format_time(time_t t, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif // RTC_DEVICE_H
