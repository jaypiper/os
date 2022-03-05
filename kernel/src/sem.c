#include <common.h>

void sem_init(sem_t *sem, const char *name, int value){
  Assert(strlen(name) < sizeof(sem->name), "sem name %s is too long", name);
  strcpy(sem->name, name);
  sem->count = value;
  sem->wait_list = NULL;
  spin_init(&sem->lock, name);
}

void sem_wait(sem_t *sem){
  spin_lock(&sem->lock);
  Assert(sem->count >= 0, "count of sem (%s) is negative (%d)", sem->name, sem->count);
  while(sem->count == 0){
    mark_not_runable(sem, cpu_current());
    spin_unlock(&sem->lock);
    yield();
    spin_lock(&sem->lock);
  }
  sem->count --;
  spin_unlock(&sem->lock);
}

void sem_signal(sem_t *sem){
  spin_lock(&sem->lock);
  Assert(sem->count >= 0, "count of sem (%s) is negative (%d)", sem->name, sem->count);
  if(sem->wait_list) wakeup_task(sem);
  sem->count ++;
  spin_unlock(&sem->lock);
}
