#include <common.h>

void pushcli(){
  int old = ienabled();
  iset(false);
  cpu_t* _cpu = get_cpu();
  Assert(_cpu->ncli >= 0, "ncli < 0 in before pushcli in cpu %d", cpu_current());
  if(_cpu->ncli == 0) _cpu->intena = old;
  _cpu->ncli ++;
}

void popcli(){
  cpu_t* _cpu = get_cpu();
  Assert(!ienabled(), "ienabled before popcli in cpu %d", cpu_current());
  Assert(_cpu->ncli > 0, "ncli <= 0 in before popcli in cpu %d", cpu_current());
  _cpu->ncli --;
  if(_cpu->ncli == 0 && _cpu->intena == 1) iset(true);
}

bool holding(spinlock_t* lk){
  pushcli();
  int ret = lk->locked && (lk->cpu_id == cpu_current());
  Assert(lk->locked == 0 || lk->locked == 1, "invalid locked %d %s\n", lk->locked, lk->name);
  popcli();
  return ret;
}

void spin_lock(spinlock_t *lk){
  pushcli();
  Assert(!holding(lk), "lock %s(%d) is already held in cpu %d", lk->name, lk->locked, cpu_current());
  uint64_t us = _sys_time();
  while(atomic_xchg(&(lk->locked), 1)){
    Assert(_sys_time() - us <= 1000000, "spin lock %s wait too long ", lk->name);
  };
  lk->cpu_id = cpu_current();

  Assert(lk->cpu_id < MAX_CPU && lk->locked <= 1, "lk %s cpu %d locked %d\n", lk->name, lk->cpu_id, lk->locked);
}

void spin_unlock(spinlock_t *lk){
  Assert(holding(lk), "lock %s is not held in cpu %d\n", lk->name, cpu_current());

  lk->cpu_id = -1;
  atomic_xchg(&(lk->locked), 0);
  popcli();

  Assert(!holding(lk), "lock %s(%d) is held after unlock in cpu %d\n", lk->name, lk->locked, cpu_current());
  Assert(lk->cpu_id < MAX_CPU && lk->locked <= 1, "lk %s cpu %d locked %d\n", lk->name, lk->cpu_id, lk->locked);
}

void spin_init(spinlock_t *lk, const char* name){
  lk->locked = 0;
  lk->cpu_id = -1;
  lk->name = name;
}

void mutex_lock(spinlock_t *lk){
  uint64_t us = _sys_time();
  while(atomic_xchg(&(lk->locked), 1)){
    Assert(_sys_time() - us <= 1000000, "mutex lock %s wait too long ", lk->name);
  };
}

void mutex_unlock(spinlock_t *lk){
  atomic_xchg(&(lk->locked), 0);
}

int mutex_trylock(spinlock_t* lk){
  return atomic_xchg(&(lk->locked), 1);
}
