#ifndef OS_KMT_H
#define OS_KMT_H
#include <sem.h>
#include <common.h>
#include <vfs.h>

enum {TASK_UNUSED = 0, TASK_RUNNING, TASK_RUNNABLE, TASK_BLOCKED, TASK_TO_BE_RUNNABLE};

typedef struct task{
  int state;
  const char* name;
  Context* context;
  struct task* wait_next;
  spinlock_t lock;
  int blocked;
  void* stack;
  ofile_info_t* ofiles[MAX_OPEN_FILE];
  int cwd_inode_no;
}task_t;

#define MAX_TASK 64
#define STACK_START_MAGIC 0x12345678
#define STACK_END_MAGIC 0x9abcdef0

#define STACK_END(task) ((uintptr_t)task->stack + STACK_SIZE - 4)
#define STACK_START(task) ((uintptr_t)task->stack + 4)
#define STACK_END_FENCE(task) (uint32_t*)STACK_END(task)
#define STACK_START_FENCE(task) (uint32_t*)task->stack

#define IS_IDLE(task) (task == idle_task[cpu_current()])

#define CHECK_TASK(task) IS_IDLE(task) || (((*STACK_START_FENCE(task) == STACK_START_MAGIC) \
                        && (*STACK_END_FENCE(task) == STACK_END_MAGIC)))

#define SET_TASK(task) \
    do { \
      *STACK_START_FENCE(task) = STACK_START_MAGIC; \
      *STACK_END_FENCE(task) = STACK_END_MAGIC; \
    } while(0)

typedef struct semaphore sem_t;

void mark_not_runable(sem_t* sem, int cpu_id);
void wakeup_task(sem_t* sem);
void* task_alloc();

#define TASK_STATE_VALID(state) ((state >= TASK_UNUSED) && (state <= TASK_BLOCKED))
#define IN_STACK(addr, task) ((uintptr_t)(addr) >= (uintptr_t)task->stack && (uintptr_t)(addr) < ((uintptr_t)task->stack + STACK_SIZE))

#endif
