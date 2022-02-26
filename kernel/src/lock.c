#include <common.h>

void spin_lock(lock_t *lk){
  uint64_t us = _sys_time();
  while(atomic_xchg(&(lk->locked), 1)){
    Assert(_sys_time() - us <= 1000000, "spin lock %s wait too long ", lk->name);
  };
}

void spin_unlock(lock_t *lk){
  Assert(lk->locked, "lock %s is already released\n", lk->name);
  atomic_xchg(&(lk->locked), 0);
}