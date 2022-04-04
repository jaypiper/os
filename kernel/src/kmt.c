#include <common.h>
#include <pmm.h>
#include <kmt.h>
#include <vfs.h>

static task_t* head = NULL;

static task_t* running_task[MAX_CPU];
static task_t* idle_task[MAX_CPU];
static task_t* last_task[MAX_CPU];
static task_t* all_task[MAX_TASK];
static Context* fork_context[MAX_TASK];
static spinlock_t task_lock;
static int total_task;

static inline task_t* get_current_task(){
  pushcli();
  task_t* ret = running_task[cpu_current()];
  popcli();
  return ret;
}

// Interrupts must be disabled.
// only called in interrupt handler
static inline void set_current_task(task_t* task){
  pushcli();
  running_task[cpu_current()] = task;
  popcli();
}

#define CURRENT_TASK get_current_task()
#define CURRENT_IDLE idle_task[cpu_current()]
#define LAST_TASK last_task[cpu_current()]

static Context* kmt_context_save(Event ev, Context * ctx){
  Assert(ctx, "saved NULL context in event %d", ev.event);
  if(!CURRENT_TASK) set_current_task(CURRENT_IDLE);

  Assert(TASK_STATE_VALID(CURRENT_TASK->state), "in context save, task %s state %d invalid", CURRENT_TASK->name, CURRENT_TASK->state);
  Assert(CURRENT_TASK->ctx_depth >= 0 && CURRENT_TASK->ctx_depth < MAX_CTX_DEPTH - 1, "context save: invalid depth %d", CURRENT_TASK->ctx_depth);
  CURRENT_TASK->contexts[CURRENT_TASK->ctx_depth ++] = ctx;
  if(CURRENT_TASK->state == TASK_RUNNING) CURRENT_TASK->state = TASK_TO_BE_RUNNABLE;

  if(LAST_TASK && LAST_TASK != CURRENT_TASK){
    if(LAST_TASK && LAST_TASK->state == TASK_TO_BE_RUNNABLE) LAST_TASK->state = TASK_RUNNABLE;
    mutex_unlock(&LAST_TASK->lock);
  }
  LAST_TASK = CURRENT_TASK;

  return NULL;
}

static Context* kmt_schedule(Event ev, Context * ctx){
  task_t* cur_task = CURRENT_TASK;
  task_t* select = cur_task && !cur_task->blocked && cur_task->state == TASK_TO_BE_RUNNABLE ? cur_task : CURRENT_IDLE;
  if(!cur_task || IS_SCHED(ev.event)){ // select a random task
    for(int i = 0; i < 8 * total_task; i++){
      int task_idx = rand() % total_task;
    // for(int task_idx = 0; task_idx < total_task; task_idx ++){
      int locked = !mutex_trylock(&all_task[task_idx]->lock);
      if(locked){
        Assert(all_task[task_idx]->state != TASK_RUNNING, "task %s running", all_task[task_idx]->name);
        if(all_task[task_idx]->state == TASK_RUNNABLE && !all_task[task_idx]->blocked){
          select = all_task[task_idx];
          break;
        }
        mutex_unlock(&all_task[task_idx]->lock);
      }
    }
  }else{ // syscall, pagefault: if not exit, keep the current task
    Assert(select == cur_task, "schedule: select %lx current %lx\n", (uintptr_t)select, (uintptr_t)cur_task);
  }
  select->state = TASK_RUNNING;
  set_current_task(select);
  Assert(TASK_STATE_VALID(select->state), "task state is invalid, name %s state %d\n", select->name, select->state);
  // Assert(CHECK_TASK(select), "task %s canary check fail", select->name);

  return select->contexts[--select->ctx_depth];
}

void kmt_init(){
  spin_init(&task_lock, "task lock");
  memset(running_task, 0, sizeof(running_task));
  memset(last_task, 0, sizeof(last_task));
  head = NULL;
  os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
  os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
  for(int i = 0; i < cpu_count(); i++){
    idle_task[i] = pmm->alloc(sizeof(task_t));
    idle_task[i]->name = "idle";
    idle_task[i]->state = TASK_RUNNING;
    idle_task[i]->stack = NULL;
    memset(idle_task[i]->contexts, 0, sizeof(idle_task[i]->contexts));
    idle_task[i]->ctx_depth = 0;
    idle_task[i]->wait_next = NULL;
    idle_task[i]->blocked = 0;
    spin_init(&idle_task[i]->lock, "idle");
  }
  memset(running_task, 0, sizeof(running_task));
  memset(fork_context, 0, sizeof(fork_context));
  total_task = 0;
  extern void vfs_proc_init();
  vfs_proc_init();
}


int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
  task->name = name;
  task->state = TASK_RUNNABLE;
  task->stack = pmm->alloc(STACK_SIZE);
  task->contexts[0] = kcontext((Area){.start = (void*)STACK_START(task->stack), .end = (void*)STACK_END(task->stack)}, entry, arg);
  task->ctx_depth = 1;
  task->wait_next = NULL;
  task->blocked = 0;
  task->as = NULL;
  memset(task->ofiles, 0, sizeof(task->ofiles));
  memset(task->mmaps, 0, sizeof(task->mmaps));
  task->cwd_inode_no = ROOT_INODE_NO;
  task->cwd_type = CWD_UFS;
  spin_init(&task->lock, name);
  SET_TASK(task);
  mutex_lock(&task_lock);
  Assert(total_task < MAX_TASK, "task full");
  all_task[total_task] = task;
  void fill_standard_fd(task_t* task);
  fill_standard_fd(task);
  new_proc_init(total_task, name);
  total_task++;
  mutex_unlock(&task_lock);
  return 0;
}

static inline void free_ofiles(task_t* task){
  for(int i = 0; i <= STDERR_FILENO; i++){
    task->ofiles[i] = NULL;
  }
  for(int i = STDERR_FILENO + 1; i < MAX_OPEN_FILE; i++){
    if(task->ofiles[i]){
      pmm->free(task->ofiles[i]);
      task->ofiles[i] = NULL;
    }
  }
}

static inline void free_mmaps(task_t* task){
  for(int i = 0; i < MAX_MMAP_NUM; i++){
    if(task->mmaps[i]){
      pmm->free(task->mmaps[i]);
      task->mmaps[i] = NULL;
    }
  }
}

static inline void free_pages(task_t* task){
  if(!task->as) return;
  unprotect(task->as);
  pmm->free(task->as);
}

void release_resources_except_stack(task_t* task){
  free_ofiles(task);
  free_mmaps(task);
  free_pages(task);
  // TODO: wakeup task
  task->wait_next = NULL;
}

void kmt_teardown(task_t *task){
  mutex_lock(&task_lock);
  int idx = -1;
  for(int i = 0; i < total_task; i++){
    if(running_task[i] == task){
      idx = i;
      break;
    }
  }
  Assert(idx != -1, "task %s not find", task->name);
  total_task --;
  for(int i = idx; i < total_task; i++){
    running_task[i] = running_task[i + 1];
  }
  Context* free_context = fork_context[idx];
  fork_context[idx] = NULL;
  mutex_unlock(&task_lock);

  release_resources_except_stack(task);
  pmm->free(task->stack);

  pmm->free((void*)task);
  pmm->free(free_context);
}

task_t* kmt_gettask(){
  return CURRENT_TASK;
}

int kmt_newforktask(task_t* newtask, const char* name){
  spin_init(&newtask->lock, name);
  newtask->name = name;
  newtask->state = TASK_RUNNABLE;
  newtask->stack = pmm->alloc(STACK_SIZE);

  SET_TASK(newtask);
  newtask->wait_next = NULL;
  newtask->blocked = 0;
  memset(newtask->ofiles, 0, sizeof(newtask->ofiles));
  memset(newtask->mmaps, 0, sizeof(newtask->mmaps));
  mutex_lock(&task_lock);
  if(total_task >= MAX_TASK){
    printf("task full");
    return -1;
  }
  if(!fork_context[total_task]){
    fork_context[total_task] = pmm->alloc(sizeof(Context));
  }
  newtask->contexts[0] = fork_context[total_task];
  newtask->ctx_depth = 1;
  all_task[total_task ++] = newtask;
  mutex_unlock(&task_lock);
  return 0;
}


MODULE_DEF(kmt) = {
  .init = kmt_init,
  .create = kmt_create,
  .teardown = kmt_teardown,
  .gettask = kmt_gettask,
  .spin_init  = spin_init,
  .spin_lock  = spin_lock,
  .spin_unlock  = spin_unlock,
  .sem_init = ksem_init,
  .sem_wait = ksem_wait,
  .sem_signal = ksem_signal
};

/* sem must be locked */
void mark_not_runable(sem_t* sem, int cpu_id){
  Assert(holding(&sem->lock), "lock in %s is not held", sem->name);
  Assert(cpu_id == cpu_current(), "in mark %s cpu_id=%d cpu current=%d", sem->name, cpu_id, cpu_current());
  Assert(running_task[cpu_id]->lock.locked, "in mark, task %s is not locked", running_task[cpu_id]->name);
  Assert(running_task[cpu_id]->state == TASK_RUNNING, "in sem mark %s, task %s is not running", sem->name, running_task[cpu_id]->name);
  CURRENT_TASK->blocked = 1;
  if(!sem->wait_list){
    sem->wait_list = running_task[cpu_id];
  } else{
    task_t* last = NULL;
    for(last = sem->wait_list; last->wait_next; last = last->wait_next){
      Assert(last->blocked, "cpu %d: task %s in sem %s is not blocked", cpu_id, last->name, sem->name);
    }
    last->wait_next = running_task[cpu_id];
  }
  running_task[cpu_id]->wait_next = NULL;
}

void wakeup_task(sem_t* sem){
  if(!sem->wait_list) return;
  task_t* select = sem->wait_list;
  Assert(select->blocked, "task %s in sem %s is not blocked", select->name, sem->name);
  sem->wait_list = select->wait_next;
  select->blocked = 0;
  select->wait_next = NULL;
}

void clear_current_task(){
  pushcli();
  set_current_task(NULL);
  popcli();
}

void* task_alloc(){
  return pmm->alloc(sizeof(task_t));
}

#ifdef KMT_DEBUG

#define PRODUCER_NUM 4
#define CONSUMER_NUM 5
#define PARENTHESIS_DEPTH 6
#define P kmt->sem_wait
#define V kmt->sem_signal

sem_t empty;
sem_t fill;

static void producer(void *arg) {
  while(1){
    P(&empty); putch('('); V(&fill);
    volatile int i = 10000;
    while(i--) ;
  }
}
static void consumer(void *arg) {
  while(1){
    P(&fill); putch(')'); V(&empty);
    volatile int i = 5000;
    while(i--) ;
  }
}

#ifdef PMM_DEBUG
#define PMM_NUM 2
static void thread_pmm(void* arg){
  extern void pmm_test();
  pmm_test();
}
#endif

void init_kmt_debug(){
  kmt->sem_init(&empty, "empty", PARENTHESIS_DEPTH);
  kmt->sem_init(&fill, "fill", 0);
  for(int i = 0; i < PRODUCER_NUM; i++){
    kmt->create(task_alloc(), "producer", producer, "producer");
  }
  for(int i = 0; i < CONSUMER_NUM; i++){
    kmt->create(task_alloc(), "consumer", consumer, "comsumer");
  }
#ifdef PMM_DEBUG
  for(int i = 0; i < PMM_NUM; i++){
    kmt->create(task_alloc(), "pmm", thread_pmm, "pmm");
  }
#endif
}

#endif
