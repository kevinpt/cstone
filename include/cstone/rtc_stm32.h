#ifndef RTC_STM32_H
#define RTC_STM32_H

// RTC clock sources
#define RTC_CLK_EXTERN_OSC    1
#define RTC_CLK_EXTERN_XTAL   2
#define RTC_CLK_INTERN        3
#define RTC_CLK_INTERN_FAST   4


#ifdef __cplusplus
extern "C" {
#endif

bool rtc_stm32_init(RTCDevice *rtc, int rtc_clk_source, uint32_t rtc_clk_freq);

#ifdef __cplusplus
}
#endif

#endif // RTC_STM32_H
