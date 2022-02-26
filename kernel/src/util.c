#include <common.h>

long long _sys_time(){
  AM_TIMER_UPTIME_T _timer = io_read(AM_TIMER_UPTIME);
  return _timer.us;
}
