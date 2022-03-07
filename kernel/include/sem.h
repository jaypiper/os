#ifndef OS_SEM_H
#define OS_SEM_H
#include <lock.h>
#include <kernel.h>

typedef struct semaphore{
  uint32_t fence1;
  int count;
  char name[32];
  spinlock_t lock;
  task_t* wait_list;
  uint32_t fence2;
} sem_t;

void sem_init(sem_t *sem, const char *name, int value);
void sem_wait(sem_t *sem);
void sem_signal(sem_t *sem);

#define SEM_FENCE1_MAGIC 0x12345
#define SEM_FENCE2_MAGIC 0x6789a
#define CHECK_SEM(sem) ((sem->fence1 == SEM_FENCE1_MAGIC) && (sem->fence2 == SEM_FENCE2_MAGIC))
#define SET_SEM(sem) do{sem->fence1 = SEM_FENCE1_MAGIC; sem->fence2 = SEM_FENCE2_MAGIC;} while(0)

#endif
