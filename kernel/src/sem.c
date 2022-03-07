#include <common.h>
#include <kmt.h>

void sem_init(sem_t *sem, const char *name, int value){
  Assert(strlen(name) < sizeof(sem->name), "sem name %s is too long", name);
  strcpy(sem->name, name);
  sem->count = value;
  sem->wait_list = NULL;
  spin_init(&sem->lock, name);
  SET_SEM(sem);
}

void sem_wait(sem_t *sem){
  Assert(CHECK_SEM(sem), "sem fence");
  spin_lock(&sem->lock);
  sem->count --;
  int is_blocked;
  if(sem->count < 0){
    mark_not_runable(sem, cpu_current());
    is_blocked = 1;
  }
  spin_unlock(&sem->lock);
  if(is_blocked){
    Assert(ienabled(), "interrupt is disabled in sem wait %s", sem->name);
    yield();
  }
  Assert(CHECK_SEM(sem), "sem fence");
}

void sem_signal(sem_t *sem){
  Assert(CHECK_SEM(sem), "sem fence");
  spin_lock(&sem->lock);
  Assert(sem->count >= 0 || sem->wait_list, "sem %s with count %d has empty wait_list", sem->name, sem->count);
  sem->count ++;
  if(sem->wait_list) wakeup_task(sem);
  spin_unlock(&sem->lock);
  Assert(CHECK_SEM(sem), "sem fence");
}

