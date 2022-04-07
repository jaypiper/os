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

  Assert(TASK_STATE_VALID(RUN_STATE(CURRENT_TASK)), "in context save, task %s state %d invalid", CURRENT_TASK->name, RUN_STATE(CURRENT_TASK));
  Assert(CURRENT_TASK->int_depth >= 0 && CURRENT_TASK->int_depth < MAX_INT_DEPTH - 1, "context save: invalid depth %d", CURRENT_TASK->int_depth);
  CURRENT_TASK->contexts[CURRENT_TASK->int_depth] = ctx;
  if(RUN_STATE(CURRENT_TASK) == TASK_RUNNING) NEXT_STATE(CURRENT_TASK) = TASK_TO_BE_RUNNABLE;
  else NEXT_STATE(CURRENT_TASK) = RUN_STATE(CURRENT_TASK);

  if(LAST_TASK && LAST_TASK != CURRENT_TASK){
    if(LAST_TASK && RUN_STATE(LAST_TASK) == TASK_TO_BE_RUNNABLE) RUN_STATE(LAST_TASK) = TASK_RUNNABLE;
    mutex_unlock(&LAST_TASK->lock);
  }
  LAST_TASK = CURRENT_TASK;
  CURRENT_TASK->int_depth ++;
  return NULL;
}

static Context* kmt_schedule(Event ev, Context * ctx){
  iset(false);
  task_t* cur_task = CURRENT_TASK;
  task_t* select = cur_task && !cur_task->blocked && (RUN_STATE(cur_task) == TASK_TO_BE_RUNNABLE) ? cur_task : CURRENT_IDLE;
  if(!cur_task || IS_SCHED(ev.event)){ // select a random task
    // for(int i = 0; i < 8 * MAX_TASK; i++){
    for(int idx = 0; idx < MAX_TASK; idx ++){
      int task_idx = ((cur_task ? cur_task->pid : 0) + idx) % MAX_TASK;
      if(!all_task[task_idx]) continue;
      int locked = !mutex_trylock(&all_task[task_idx]->lock);
      if(locked){
        Assert(RUN_STATE(all_task[task_idx]) != TASK_RUNNING, "task %s running", all_task[task_idx]->name);
        if(RUN_STATE(all_task[task_idx]) == TASK_RUNNABLE && !all_task[task_idx]->blocked){
          select = all_task[task_idx];
          break;
        }
        mutex_unlock(&all_task[task_idx]->lock);
      }
    }
  }else{ // syscall, pagefault: if not exit, keep the current task
    Assert(select == cur_task, "schedule: select %lx current %lx\n", (uintptr_t)select, (uintptr_t)cur_task);
  }

  set_current_task(select);
  Assert(TASK_STATE_VALID(RUN_STATE(select)), "task state is invalid, name %s state %d\n", select->name, RUN_STATE(select));
  // Assert(CHECK_TASK(select), "task %s canary check fail", select->name);

  select->int_depth --;
  return select->contexts[select->int_depth];
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
    idle_task[i]->states[0] = TASK_RUNNING;
    idle_task[i]->stack = NULL;
    idle_task[i]->kstack = idle_task[i]->stack;
    memset(idle_task[i]->contexts, 0, sizeof(idle_task[i]->contexts));
    idle_task[i]->int_depth = 0;
    idle_task[i]->wait_next = NULL;
    idle_task[i]->blocked = 0;
    spin_init(&idle_task[i]->lock, "idle");
  }
  memset(running_task, 0, sizeof(running_task));
  memset(fork_context, 0, sizeof(fork_context));
  extern void vfs_proc_init();
  vfs_proc_init();
}

static inline int get_empty_pid(){
  for(int i = 0; i < MAX_TASK; i++){
    if(!all_task[i]) return i;
  }
  Assert(0, "task full\n");
  return -1;
}

int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
  task->name = name;
  task->states[0] = TASK_RUNNING;
  task->states[1] = TASK_RUNNABLE;
  task->stack = pmm->alloc(STACK_SIZE);
  task->kstack = task->stack;
  task->contexts[0] = kcontext((Area){.start = (void*)STACK_START(task->stack), .end = (void*)STACK_END(task->stack)}, entry, arg);
  task->int_depth = 1;
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
  int pid = get_empty_pid();
  task->pid = pid;
  all_task[pid] = task;
  fill_standard_fd(task);
  new_proc_init(pid, name);
  mutex_unlock(&task_lock);
  return 0;
}

static inline void free_ofiles(task_t* task){
  for(int i = 0; i <= STDERR_FILENO; i++){
    task->ofiles[i] = NULL;
  }
  for(int i = STDERR_FILENO + 1; i < MAX_OPEN_FILE; i++){
    if(task->ofiles[i]){
      fileclose(task->ofiles[i]);
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

void free_pages(AddrSpace* as){
  if(!as) return;
  unprotect(as);
  pmm->free(as);
}

void release_resources(task_t* task){
  free_ofiles(task);
  free_mmaps(task);
  free_pages(task->as);

  task->wait_next = NULL;
  pmm->free(task->stack);
  pmm->free((void*)task);
}

void execve_release_resources(task_t* task){
  free_ofiles(task);
  free_mmaps(task);

  task->wait_next = NULL;
}

void kmt_teardown(task_t *task){
  mutex_lock(&task_lock);
  Assert(!task->blocked, "blocked task should not be teardown");
  int pid = task->pid;
  Assert(all_task[pid] == task, "teardown: task with pid %d mismatched", pid);
  all_task[pid] = NULL;
  Context* free_context = fork_context[pid];
  fork_context[pid] = NULL;
  mutex_unlock(&task_lock);

  if(task == CURRENT_TASK){
    set_current_task(NULL);
  }else{
    mutex_lock(&task_lock);  // insure task is not running on another CPU
  }

  release_resources(task);
  delete_proc(pid);
  if(free_context) pmm->free(free_context);
}

task_t* kmt_gettask(){
  return CURRENT_TASK;
}

int kmt_initforktask(task_t* newtask, const char* name){
  spin_init(&newtask->lock, name);
  newtask->name = name;
  newtask->states[0] = TASK_RUNNING;
  newtask->states[1] = TASK_RUNNABLE;
  newtask->stack = pmm->alloc(STACK_SIZE);
  newtask->contexts[0] = pmm->alloc(sizeof(Context));

  SET_TASK(newtask);
  newtask->wait_next = NULL;
  newtask->blocked = 0;
  newtask->int_depth = 1;
  memset(newtask->ofiles, 0, sizeof(newtask->ofiles));
  memset(newtask->mmaps, 0, sizeof(newtask->mmaps));

  return 0;
}

void kmt_inserttask(task_t* newtask){
  mutex_lock(&task_lock);
  int pid = get_empty_pid();

  fork_context[pid] = newtask->contexts[0];

  newtask->pid = pid;

  all_task[pid] = newtask;
  mutex_unlock(&task_lock);
  new_proc_init(pid, newtask->name);
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
