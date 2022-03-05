#ifndef OS_KMT_H
#define OS_KMT_H
#include <sem.h>

enum {TASK_UNUSED = 0, TASK_RUNNING, TASK_RUNNABLE, TASK_BLOCKED};

typedef struct task{
  int state;
  const char* name;
  Context* context;
  struct task* next;
  struct task* wait_next;
  Area stack;
  long long time;
}task_t;

#define MAX_THREAD 64
#define CANARY_MAGIC 0x12345678
#define CANARY(task) (uint32_t*)(task->stack.start)
typedef struct semaphore sem_t;

void set_idle_thread();
void mark_not_runable(sem_t* sem, int cpu_id);
void wakeup_task(sem_t* sem);

#endif
