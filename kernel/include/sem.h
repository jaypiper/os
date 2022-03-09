#ifndef OS_SEM_H
#define OS_SEM_H
#include <lock.h>
#include <kernel.h>

typedef struct semaphore{
  int count;
  const char* name;
  spinlock_t lock;
  task_t* wait_list;
} sem_t;

void ksem_init(sem_t *sem, const char *name, int value);
void ksem_wait(sem_t *sem);
void ksem_signal(sem_t *sem);

#define SEM_FENCE1_MAGIC 0x12345
#define SEM_FENCE2_MAGIC 0x6789a

#endif
