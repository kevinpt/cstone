#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>

#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_rtc.h"

#include "cstone/rtc_device.h"
#include "cstone/rtc_stm32.h"
#include "cstone/prop_id.h"
#include "cstone/debug.h"
#include "util/crc8.h"

#define RTC_LSE_CLK   32768
#define RTC_LSI_CLK   32000

#define USE_LSI_CLK
#ifndef USE_LSI_CLK
#  define RTC_CLK       RTC_LSE_CLK
#else
#  define RTC_CLK       RTC_LSI_CLK
#endif


static void disable_rtc_write_protect(void) {
  LL_RTC_DisableWriteProtection(RTC);
  // NOTE: A small delay is required before protect is actually off
  for(volatile int i = 0; i < 2; i++) {
    __NOP();
  }
}


static void save_init_time(time_t time) {
  uint8_t crc = crc8_init();
  crc = crc8_update_small_block(crc, (uint8_t *)&time, sizeof time);
  crc = crc8_finish(crc);

  LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR0, time & 0xFFFFFFFF);
  LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR1, time >> 32);
  LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR2, crc);
}


static bool get_init_time(time_t *init_time) {
  time_t bak_time;
  bak_time = LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR0);
  bak_time = ((time_t)LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR1) << 32) | bak_time;
  uint8_t bak_crc = LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR2);

  // Recompute CRC to verify valid data
  uint8_t crc = crc8_init();
  crc = crc8_update_small_block(crc, (uint8_t *)&bak_time, sizeof bak_time);
  crc = crc8_finish(crc);

  *init_time = bak_time;
  return crc == bak_crc;
}


static bool rtc__stm32_valid_time(RTCDevice *rtc) {
  time_t t;
  return get_init_time(&t);
}


static void rtc__stm32_set_time(RTCDevice *rtc, time_t time) {
  struct tm now;

  localtime_r(&time, &now);

  LL_RTC_TimeTypeDef rtime = {
    .TimeFormat = LL_RTC_TIME_FORMAT_AM_OR_24,
    .Hours    = now.tm_hour,
    .Minutes  = now.tm_min,
    .Seconds  = now.tm_sec
  };

  LL_RTC_DateTypeDef rdate = {
    .WeekDay  = now.tm_wday == 0 ? LL_RTC_WEEKDAY_SUNDAY : now.tm_wday,
    .Day      = now.tm_mday,
    .Month    = now.tm_mon + 1,
    .Year     = now.tm_year + 1900 - 2000
  };

  

  LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &rtime);
  LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &rdate);

  if((int)LL_RTC_TIME_IsDayLightStoreEnabled(RTC) != now.tm_isdst) {
    disable_rtc_write_protect();

    now.tm_isdst == 1 ? LL_RTC_TIME_EnableDayLightStore(RTC) : LL_RTC_TIME_DisableDayLightStore(RTC);
    LL_RTC_EnableWriteProtection(RTC);
  }

  save_init_time(time);
}


static time_t rtc__stm32_get_time(RTCDevice *rtc) {
  uint32_t rdate;
  uint32_t rtime;

  if(LL_RTC_IsShadowRegBypassEnabled(RTC)) {
    rdate = LL_RTC_DATE_Get(RTC);
    rtime = LL_RTC_TIME_Get(RTC);

    uint32_t rdate2 = LL_RTC_DATE_Get(RTC);
    if(rdate2 != rdate) { // Day rollover since getting time
      rtime = LL_RTC_TIME_Get(RTC);
      rdate = rdate2;
    }

  } else {  // Using shadow registers
    while(!LL_RTC_IsActiveFlag_RS(RTC));

    rtime = LL_RTC_TIME_Get(RTC); // Lock date in shadow DR
    rdate = LL_RTC_DATE_Get(RTC);
    LL_RTC_ClearFlag_RS(RTC);

  }

  struct tm now = {
    .tm_year = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_YEAR(rdate)) + 2000 - 1900,
    .tm_mon  = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MONTH(rdate)) - 1,
    .tm_mday = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_DAY(rdate)),
    .tm_hour = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_HOUR(rtime)),
    .tm_min  = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MINUTE(rtime)),
    .tm_sec  = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_SECOND(rtime)),
    .tm_isdst = -1
  };

  if(LL_RTC_GetHourFormat(RTC) == LL_RTC_HOURFORMAT_AMPM) { // Convert to 24h
    if(READ_BIT(rtime, RTC_TR_PM) == LL_RTC_TIME_FORMAT_PM) {
      if(now.tm_hour != 12)
        now.tm_hour += 12;
    } else if(now.tm_hour == 12) {  // 12:00AM - 12:59AM --> 00:00 - 00:59
      now.tm_hour = 0;
    }
  }

  return mktime(&now);
}


static bool rtc__stm32_calibrate(RTCDevice *rtc, int cal_error, RTCCalibrateOp cal_op) {
  // https://gist.github.com/esynr3z/8284f6dc10feab1e00609f26e42b974c
  // This uses the STM32F4 soft calibration mechanism. Other STM32 families work differently.

  bool dry_run = cal_op & RTC_CAL_DRY_RUN;

  if(!dry_run && (cal_op & RTC_CAL_CLEAR)) {
    // Wipe cal settings
    while(LL_RTC_IsActiveFlag_RECALP(RTC));
    disable_rtc_write_protect();
    RTC->CALR = 0;
    LL_RTC_EnableWriteProtection(RTC);

    puts("RTC Calibration cleared");
    return true;
  }

  if(!(cal_op & RTC_CAL_SET)) // Unknown operation
    return false;


  time_t now = rtc__stm32_get_time(rtc);

  // Retrieve initial time from backup registers
  time_t init_time;
  if(!get_init_time(&init_time))
    return false;

  if(cal_error >= 2048 || cal_error < -2048) // calm calculation will overflow
    return false;

  now -= cal_error; // Set to correct time

  if(init_time >= now)  // Invalid init time
    return false;


  // Apply correction to current time (Also sets new init time)
  if(!dry_run && cal_error != 0)
    rtc__stm32_set_time(rtc, now);



  // Calculate new correction value
  int32_t elapsed = (int32_t)(now - init_time);
//  int32_t err_ppm = ((int32_t)cal_error * CAL_CYCLE) / elapsed;
//  int32_t err_freq = ((RTC_CLK * err_ppm + CAL_CYCLE/2) / CAL_CYCLE);

  /* STM32F4 RTC can suppress up to 511 clock cycles using the CALM field. It can also
     inject 512 extra clock cycles when CALP is enabled. This permits adjustment of
     clocks that run fast *and* slow by -487.1ppm and +488.5ppm.

     When cal_error is negative the clock is running slow so we enable CALP and use CALM
     to back off from the extra 512 cycles.
  */

  // Get old calibration settings
  bool old_calp = LL_RTC_CAL_IsPulseInserted(RTC);
  int32_t old_calm = LL_RTC_CAL_GetMinus(RTC);
  if(old_calp)  // Revert to negative adjustment
    old_calm = old_calm - 512;

#define CAL_CYCLE   (1L << 20)  // 2^20 pulses  ≈ 1M
  //int32_t calm = (CAL_CYCLE * err_freq) / RTC_CLK;  // Number of cycles to add or remove
  int32_t calm = (CAL_CYCLE * cal_error) / elapsed;  // Number of cycles to add or remove
  calm += old_calm; // Incorporate existing cal offset

//  int32_t calm_alt = (CAL_CYCLE * cal_error) / elapsed;
//  DPRINT("CALM_ALT:%d\n", calm_alt);

  if(DEBUG_FEATURE(PF_DEBUG_SYS_LOCAL_RTCCAL)) {
    puts(FLAG_PREFIX(RTCCAL));
    char buf[20];
    format_time(init_time, buf, sizeof buf);
    printf("\tLast cal: %s\n", buf);
    format_time(now, buf, sizeof buf);
    printf("\tNow:      %s\n", buf);
    int32_t err_cycles = (RTC_CLK * cal_error) / elapsed;
    printf("\tElapsed=%ld,  Err. cycles=%ld,  Eff. freq=%ld\n", elapsed, err_cycles, RTC_CLK + err_cycles);
    printf("\tOld CALM=%ld, New CALM=%ld" A_NONE "\n", old_calm, calm);
  }

  if(calm < -512 || calm > 511) { // Out of range
    puts("Calibration factor out of range");
    return false;
  }

  bool calp = false;
  if(calm < 0) {  // We need to add 512 cycles with calp then subtract off the excess
    calp = true;
    calm = 512 + calm;
  }

  if(!dry_run && cal_error != 0) {  // Apply new calibration settings
    while(LL_RTC_IsActiveFlag_RECALP(RTC));
    disable_rtc_write_protect();

    // We can't use the LL API to set both CALM and CALP with individual calls because the first
    // will trigger a recal cycle and block the second write.
    // We have to write to CALR in one instruction:
    //LL_RTC_CAL_SetMinus(RTC, calm);
    //LL_RTC_CAL_SetPulse(RTC, calp ? LL_RTC_CALIB_INSERTPULSE_SET : LL_RTC_CALIB_INSERTPULSE_NONE);
    RTC->CALR = (calp ? LL_RTC_CALIB_INSERTPULSE_SET : LL_RTC_CALIB_INSERTPULSE_NONE) | calm;
    LL_RTC_EnableWriteProtection(RTC);

#if 0
    old_calp = LL_RTC_CAL_IsPulseInserted(RTC);
    old_calm = LL_RTC_CAL_GetMinus(RTC);

    if(old_calp != calp)  puts("ERROR: CALP mismatch");
    if(old_calm != calm)  printf("ERROR: CALM mismatch %ld != %ld  (%d, %d)\n", old_calm, calm, old_calp, calp);
#endif
  }

  return true;
}


bool rtc_stm32_init(RTCDevice *rtc) {
  rtc->set_time   = rtc__stm32_set_time;
  rtc->get_time   = rtc__stm32_get_time;
  rtc->valid_time = rtc__stm32_valid_time;
  rtc->calibrate  = rtc__stm32_calibrate;

  __HAL_RCC_PWR_CLK_ENABLE();
  LL_PWR_EnableBkUpAccess();
  LL_PWR_EnableBkUpAccess();

//  LL_RCC_ForceBackupDomainReset();
//  LL_RCC_ReleaseBackupDomainReset();

#ifdef USE_LSI_CLK
  __HAL_RCC_LSI_ENABLE();
  while(!LL_RCC_LSI_IsReady());
#else
  LL_RCC_LSE_EnableBypass(); // External oscillator
  LL_RCC_LSE_Enable();

  while(!LL_RCC_LSE_IsReady());
#endif

#ifdef USE_LSI_CLK
  LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
#else
  LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);
#endif

  LL_RCC_EnableRTC();

#if 1
  // Only initialize the RTC if it isn't already running. This prevents 
  if(!LL_RTC_IsActiveFlag_INITS(RTC)) {
    LL_RTC_InitTypeDef cfg = {
      .HourFormat       = LL_RTC_HOURFORMAT_24HOUR,
      .AsynchPrescaler  = 128-1, // Subseconds tick ~256Hz
      .SynchPrescaler   = (RTC_CLK / 128)-1
    };

    LL_RTC_Init(RTC, &cfg);
  }
#endif
  return true;
}

