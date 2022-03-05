#include <common.h>

void spin_lock(spinlock_t *lk){
  lk->ienabled = ienabled();
  iset(false);
  uint64_t us = _sys_time();
  while(atomic_xchg(&(lk->locked), 1)){
    Assert(_sys_time() - us <= 1000000, "spin lock %s wait too long ", lk->name);
  };
}

void spin_unlock(spinlock_t *lk){
  Assert(lk->locked, "lock %s is already released\n", lk->name);
  atomic_xchg(&(lk->locked), 0);
  iset(lk->ienabled);
}

void spin_init(spinlock_t *lk, const char* name){
  Assert(strlen(name) < sizeof(lk->name), "spinlock name %s is too long", name);
  lk->locked = 0;
  strcpy(lk->name, name);
}