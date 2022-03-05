#ifndef OS_SEM_H
#define OS_SEM_H
#include <lock.h>
#include <kmt.h>

typedef struct semaphore{
  int count;
  char name[32];
  spinlock_t lock;
  task_t* wait_list;
} sem_t;

void sem_init(sem_t *sem, const char *name, int value);
void sem_wait(sem_t *sem);
void sem_signal(sem_t *sem);

#endif
