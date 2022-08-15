#include <common.h>
#include <pmm.h>
#include <kmt.h>
#include <vfs.h>

static task_t* head = NULL;

extern void set_timer();

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
  if(ev.event == EVENT_IRQ_TIMER) set_timer();
  if(!CURRENT_TASK) set_current_task(CURRENT_IDLE);

  Assert(TASK_STATE_VALID(RUN_STATE(CURRENT_TASK)), "in context save, task %s state %d invalid", CURRENT_TASK->name, RUN_STATE(CURRENT_TASK));
  Assert(CURRENT_TASK->int_depth >= 0 && CURRENT_TASK->int_depth < MAX_INT_DEPTH - 1, "context save: invalid depth %d", CURRENT_TASK->int_depth);
  CURRENT_TASK->contexts[CURRENT_TASK->int_depth] = ctx;
  if(RUN_STATE(CURRENT_TASK) == TASK_RUNNING || RUN_STATE(CURRENT_TASK) == TASK_TO_BE_RUNNABLE) NEXT_STATE(CURRENT_TASK) = TASK_TO_BE_RUNNABLE;
  // else NEXT_STATE(CURRENT_TASK) = RUN_STATE(CURRENT_TASK);

  if(LAST_TASK && LAST_TASK != CURRENT_TASK){
    if(LAST_TASK && RUN_STATE(LAST_TASK) == TASK_TO_BE_RUNNABLE){
      RUN_STATE(LAST_TASK) = TASK_RUNNABLE;
    }
    if(RUN_STATE(LAST_TASK) == TASK_DEAD){
      kmt->teardown(LAST_TASK);
    }else{
      mutex_unlock(&LAST_TASK->lock);
    }
  }

  LAST_TASK = CURRENT_TASK;
  CURRENT_TASK->int_depth ++;
  return NULL;
}

static Context* kmt_schedule(Event ev, Context * ctx){
  iset(false);
  task_t* cur_task = CURRENT_TASK;
  task_t* select = cur_task && !cur_task->blocked && (RUN_STATE(cur_task) == TASK_TO_BE_RUNNABLE) ? cur_task : CURRENT_IDLE;
  if(RUN_STATE(cur_task) == TASK_DEAD || RUN_STATE(cur_task) == TASK_WAIT || IS_SCHED(ev.event)){ // select a random task
    spin_lock(&task_lock);
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
    spin_unlock(&task_lock);
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
  memset(all_task, 0, sizeof(all_task));
  memset(idle_task, 0, sizeof(idle_task));
  memset(fork_context, 0, sizeof(fork_context));
  head = NULL;
  os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
  os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
  for(int i = 0; i < cpu_count(); i++){
    idle_task[i] = pmm->alloc(sizeof(task_t));
    strcpy(idle_task[i]->name, "idle");
    idle_task[i]->states[0] = TASK_RUNNING;
    idle_task[i]->stack = NULL;
    idle_task[i]->kstack = idle_task[i]->stack;
    idle_task[i]->max_brk = NULL;
    memset(idle_task[i]->contexts, 0, sizeof(idle_task[i]->contexts));
    idle_task[i]->int_depth = 0;
    idle_task[i]->wait_next = NULL;
    idle_task[i]->blocked = 0;
    idle_task[i]->pid = idle_task[i]->tgid = idle_task[i]->ppid = 0;
    spin_init(&idle_task[i]->lock, "idle");
  }
}

int get_empty_pid(){
  spin_lock(&task_lock);
  for(int i = 1; i < MAX_TASK; i++){
    if(!all_task[i]) {
      spin_unlock(&task_lock);
      return i;
    }
  }
  spin_unlock(&task_lock);
  Assert(0, "task full\n");
  return -1;
}

task_t* task_by_pid(int pid){
  return all_task[pid];
}

int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
  Assert(strlen(name) < MAX_TASKNAME_LEN, "name %s is too long", name);
  strcpy(task->name, name);
  task->states[0] = TASK_RUNNING;
  task->states[1] = TASK_RUNNABLE;
  task->stack = pmm->alloc(STACK_SIZE);
  task->kstack = task->stack;
  task->max_brk = NULL;
  task->mmap_end = NULL;
  task->contexts[0] = kcontext((Area){.start = (void*)STACK_START(task->stack), .end = (void*)STACK_END(task->stack)}, entry, arg);
  task->int_depth = 1;
  task->wait_next = NULL;
  task->blocked = 0;
  task->as = pmm->alloc(sizeof(AddrSpace));
  init_kernel_as(task->as);
  memset(task->ofiles, 0, sizeof(task->ofiles));
  memset(task->mmaps, 0, sizeof(task->mmaps));
  init_task_cwd(task);
  spin_init(&task->lock, name);
  SET_TASK(task);

  task->pid = task->tgid = get_empty_pid();
  task->ppid = -1;
  spin_lock(&task_lock);
  all_task[task->pid] = task;

  fill_standard_fd(task);
  spin_unlock(&task_lock);
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
  if(task->stack != task->kstack) pmm->free(task->kstack);
  pmm->free(task->stack);
  for(int i = 0; i < STACK_SIZE / PGSIZE; i++){
    map(task->as, task->as->area.end - STACK_SIZE + i * PGSIZE, 0, MMAP_NONE);
  }

  free_ofiles(task);
  free_mmaps(task);
  free_pages(task->as);

  task->wait_next = NULL;
  pmm->free((void*)task);
}

void execve_release_resources(task_t* task){
  free_ofiles(task);
  free_mmaps(task);

  task->wait_next = NULL;
}

void kmt_teardown(task_t *task){
  spin_lock(&task_lock);
  Assert(!task->blocked, "blocked task should not be teardown");
  int pid = task->pid;
  Assert(all_task[pid] == task, "teardown: task with pid %d mismatched", pid);
  all_task[pid] = NULL;
  Context* free_context = fork_context[pid];
  fork_context[pid] = NULL;
  release_resources(task);
  if(free_context) pmm->free(free_context);
  spin_unlock(&task_lock);
}

void kmt_teardown_group(int tgid){
  spin_lock(&task_lock);
  for(int i = 0; i < MAX_TASK; i++){
    task_t* select = all_task[i];
    if(!select) continue;
    if(select->tgid == tgid){
      if(RUN_STATE(select) == TASK_DEAD) continue;
      if(fork_context[i]) pmm->free(fork_context[i]);
      release_resources(select);
    }
  }
  spin_unlock(&task_lock);
}

task_t* kmt_gettask(){
  return CURRENT_TASK;
}

int kmt_initforktask(task_t* newtask, const char* name){
  spin_init(&newtask->lock, name);
  Assert(strlen(name) < MAX_TASKNAME_LEN, "name %s is too long", name);
  strcpy(newtask->name, name);
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

void kmt_inserttask(task_t* newtask, int is_fork){
  spin_lock(&task_lock);
  all_task[newtask->pid] = newtask;
  if(is_fork) fork_context[newtask->pid] = newtask->contexts[0];

  spin_unlock(&task_lock);
}

MODULE_DEF(kmt) = {
  .init = kmt_init,
  .create = kmt_create,
  .teardown = kmt_teardown,
  .teardown_group = kmt_teardown_group,
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

void notify_parent(int pid, int wstatus){
  if(pid < 0) return;
  mutex_lock(&all_task[pid]->lock);
  all_task[pid]->wstatus = wstatus;
  if(RUN_STATE(all_task[pid]) == TASK_WAIT) RUN_STATE(all_task[pid]) = TASK_RUNNABLE;
  mutex_unlock(&all_task[pid]->lock);
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
