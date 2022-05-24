#include <am.h>

#define TIME_INTERVAL (403000000 / 100)

void __am_timer_init() {
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uptime->us = 0;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}

uint64_t r_time()
{
  uint64_t x;
  asm volatile("rdtime %0" : "=r" (x) );
  return x;
}

void set_timer(){
  sbi_set_timer(r_time() + TIME_INTERVAL);
}
