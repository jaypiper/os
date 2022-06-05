#ifndef OS_KMT_H
#define OS_KMT_H
#include <sem.h>
#include <common.h>
#include <fat32.h>
#include <uproc.h>

enum {TASK_UNUSED = 0, TASK_RUNNING, TASK_RUNNABLE, TASK_BLOCKED, TASK_TO_BE_RUNNABLE, TASK_DEAD};

#define MAX_INT_DEPTH 5

#define TOP_CONTEXT(task) task->contexts[task->int_depth-1]
#define RUN_STATE(task) task->states[task->int_depth]
#define NEXT_STATE(task) task->states[task->int_depth+1]

typedef struct task{
  int states[MAX_INT_DEPTH];
  int pid;
  const char* name;
  Context* contexts[MAX_INT_DEPTH];
  int int_depth;
  struct task* wait_next;
  spinlock_t lock;
  int blocked;
  void* stack;
  void* kstack;
  void* max_brk;
  void* brk;
  ofile_t* ofiles[MAX_OPEN_FILE];
  mm_area_t* mmaps[MAX_MMAP_NUM];
  dirent_t* cwd;
  int cwd_type;
  AddrSpace* as;
}task_t;

#define MAX_TASK 64
#define STACK_START_MAGIC 0x12345678
#define STACK_END_MAGIC 0x9abcdef0

#define STACK_END(stack) ((uintptr_t)stack + STACK_SIZE)
#define STACK_START(stack) ((uintptr_t)stack)
#define STACK_END_FENCE(task) (uint32_t*)STACK_END(task)
#define STACK_START_FENCE(task) (uint32_t*)task->stack

#define IS_IDLE(task) (task == idle_task[cpu_current()])


#if 0
#define CHECK_TASK(task) IS_IDLE(task) || (((*STACK_START_FENCE(task) == STACK_START_MAGIC) \
                        && (*STACK_END_FENCE(task) == STACK_END_MAGIC)))

#define SET_TASK(task) \
    do { \
      *STACK_START_FENCE(task) = STACK_START_MAGIC; \
      *STACK_END_FENCE(task) = STACK_END_MAGIC; \
    } while(0)
#else
#define SET_TASK(task)
#define CHECK_TASK(task)
#endif

typedef struct semaphore sem_t;

void mark_not_runable(sem_t* sem, int cpu_id);
void wakeup_task(sem_t* sem);
void* task_alloc();
int kmt_initforktask(task_t* newtask, const char* name);
void kmt_inserttask(task_t* newtask, int is_fork);
void clear_current_task();
void release_resources(task_t* task);
void execve_release_resources(task_t* task);
void free_pages(AddrSpace* as);

#define TASK_STATE_VALID(state) ((state >= TASK_UNUSED) && (state <= TASK_DEAD))
#define IN_STACK(addr, task) ((uintptr_t)(addr) >= (uintptr_t)task->stack && (uintptr_t)(addr) < ((uintptr_t)task->stack + STACK_SIZE))

#define IS_IRQ(event) (event == EVENT_IRQ_TIMER || event == EVENT_IRQ_IODEV)
#define IS_SCHED(event) (IS_IRQ(event) || event == EVENT_YIELD)
#endif
