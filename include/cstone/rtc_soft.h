#ifndef RTC_SOFT_H
#define RTC_SOFT_H


#ifdef __cplusplus
extern "C" {
#endif

void rtc_soft_update(RTCDevice *rtc, int seconds);
bool rtc_soft_init(RTCDevice *rtc);

#ifdef __cplusplus
}
#endif

#endif // RTC_SOFT_H
