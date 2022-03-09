#ifndef OS_KMT_H
#define OS_KMT_H
#include <sem.h>
#include <common.h>

enum {TASK_UNUSED = 0, TASK_RUNNING, TASK_RUNNABLE, TASK_BLOCKED, TASK_TO_BE_RUNNABLE};

typedef struct task{
  uint32_t fence1;
  int state;
  const char* name;
  Context* context;
  struct task* wait_next;
  spinlock_t lock;
  int blocked;
  uint32_t fence2;
  void* stack;
  uint32_t fence3;
}task_t;

#define MAX_TASK 64
#define TASK_FENCE1_MAGIC 0x12345678
#define TASK_FENCE2_MAGIC 0x56789abc
#define TASK_FENCE3_MAGIC 0x9abcdef0

#define CHECK_TASK(task) ((task->fence1 == TASK_FENCE1_MAGIC) \
                      && (task->fence2 == TASK_FENCE2_MAGIC) \
                      && (task->fence3 == TASK_FENCE3_MAGIC))

#define SET_TASK(task) \
    do { \
      task->fence1 = TASK_FENCE1_MAGIC; \
      task->fence2 = TASK_FENCE2_MAGIC; \
      task->fence3 = TASK_FENCE3_MAGIC; \
    } while(0)

typedef struct semaphore sem_t;

void mark_not_runable(sem_t* sem, int cpu_id);
void wakeup_task(sem_t* sem);
void* task_alloc();

#define TASK_STATE_VALID(state) ((state >= TASK_UNUSED) && (state <= TASK_BLOCKED))
#define IN_STACK(addr, task) ((uintptr_t)(addr) >= (uintptr_t)task->stack && (uintptr_t)(addr) < ((uintptr_t)task->stack + STACK_SIZE))
#endif
