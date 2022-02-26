#include <common.h>

static long long _sys_time(){
  AM_TIMER_UPTIME_T _timer = io_read(AM_TIMER_UPTIME);
  return _timer.us;
}

void spin_lock(lock_t *lk){
  uint64_t us = _sys_time();
  while(atomic_xchg(&(lk->locked), 1)){
    Assert(_sys_time() - us <= 1000000, "spin lock %s wait too long ", lk->name);
  };
}

void spin_unlock(lock_t *lk){
  int ret = atomic_xchg(&(lk->locked), 0);
  Assert(ret, "lock %s is already released\n", lk->name);
}