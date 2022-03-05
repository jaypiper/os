#include <common.h>
#include <pmm.h>

static task_t* head = NULL;

static task_t* running_task[MAX_CPU];
static long long running_time[MAX_CPU];
static spinlock_t task_lock;
static int total_task;

static Context* kmt_context_save(Event ev, Context * ctx){
  Assert(ctx, "saved NULL context in event %d", ev.event);
  int cpu_id = cpu_current();
  long long cur_time = _sys_time();
  Assert(running_time[cpu_id] <= cur_time, "time is out of bound, running time %ld cur_time %ld", running_time[cpu_id], cur_time);

  spin_lock(&task_lock);
  Assert(TASK_STATE_VALID(running_task[cpu_id]->state), "in context save, task %s state %d invalid", running_task[cpu_id]->name, running_task[cpu_id]->state);
  running_task[cpu_id]->context = ctx;
  running_task[cpu_id]->time += cur_time - running_time[cpu_id];
  if(running_task[cpu_id]->state == TASK_RUNNING) running_task[cpu_id]->state = TASK_RUNNABLE;
  spin_unlock(&task_lock);
  return NULL;
}

static Context* kmt_schedule(Event ev, Context * ctx){
  int cpu_id = cpu_current();
  long long min_time = LLONG_MAX;
  task_t* select = NULL;
  spin_lock(&task_lock);
  int cur_task = 0;
  for(task_t* h = head; h; h = h->next){
    Assert(*CANARY(h) == CANARY_MAGIC, "task %s stack overflow", h->name);
    if(h->time < min_time && h->state == TASK_RUNNABLE){
      select = h;
      min_time = h->time;
    }
    cur_task ++;
    Assert(TASK_STATE_VALID(h->state), "task state is invalid, name %s state %d\n", h->name, h->state);
  }
  Assert(cur_task == total_task, "expect %d tasks, find %d", total_task, cur_task);
  Assert(select, "no available task");
  select->state = TASK_RUNNING;
  running_task[cpu_id] = select;
  running_time[cpu_id] = _sys_time();
  spin_unlock(&task_lock);
  return select->context;
}

void kmt_init(){
  spin_init(&task_lock, "thread lock");
  memset(running_task, 0, sizeof(running_task));
  memset(running_time, 0, sizeof(running_time));
  head = NULL;
  os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
  os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
  total_task = 0;
}


int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
  task->stack.start = pmm->alloc(STACK_SIZE);
  task->stack.end = task->stack.start + STACK_SIZE;
  task->name = name;
  task->state = TASK_RUNNABLE;
  task->context = kcontext(task->stack, entry, arg);
  task->wait_next = NULL;
  task->time = 0;
  *CANARY(task) = CANARY_MAGIC;
  spin_lock(&task_lock);
  task->next = head;
  head = task;
  total_task ++;
  spin_unlock(&task_lock);
  return 0;
}

void kmt_teardown(task_t *task){
  Assert(head, "no task");
  spin_lock(&task_lock);
  if(head == task){
    head = head->next;
  }else{
    task_t* h = head;
    while(h->next && (h->next != task)) h = h->next;
    Assert(h, "task %s not find", task->name);
    h->next = task->next;
  }
  total_task --;
  spin_unlock(&task_lock);
  pmm->free((void*)task->stack.start);
}

void set_idle_thread(){
  int cpu_id = cpu_current();
  task_t* idle_task = pmm->alloc(sizeof(task_t));

  idle_task->stack.start = pmm->alloc(STACK_SIZE);
  idle_task->stack.end = idle_task->stack.start + STACK_SIZE;
  idle_task->name = "idle";
  idle_task->state = TASK_RUNNING;
  idle_task->wait_next = NULL;
  idle_task->time = 0;
  *CANARY(idle_task) = CANARY_MAGIC;
  spin_lock(&task_lock);
  running_task[cpu_id] = idle_task;
  idle_task->next = head;
  head = idle_task;
  total_task ++;
  spin_unlock(&task_lock);
}

MODULE_DEF(kmt) = {
  .init = kmt_init,
  .create = kmt_create,
  .teardown = kmt_teardown,
  .spin_init  = spin_init,
  .spin_lock  = spin_lock,
  .spin_unlock  = spin_unlock,
  .sem_init = sem_init,
  .sem_wait = sem_wait,
  .sem_signal = sem_signal
};

/* sem must be locked */
void mark_not_runable(sem_t* sem, int cpu_id){
  spin_lock(&task_lock);
  running_task[cpu_id]->state = TASK_BLOCKED;
  if(!sem->wait_list){
    sem->wait_list = running_task[cpu_id];
  } else{
    task_t* last = NULL;
    for(last = sem->wait_list; last->wait_next; last = last->wait_next){
      Assert(last->state == TASK_BLOCKED, "cpu %d: task %s in sem %s is not blocked", cpu_id, last->name, sem->name);
    }
    last->wait_next = running_task[cpu_id];
  }
  running_task[cpu_id]->wait_next = NULL;
  spin_unlock(&task_lock);
}

void wakeup_task(sem_t* sem){
  if(!sem->wait_list) return;
  spin_lock(&task_lock);
  task_t* select = sem->wait_list;
  Assert(select->state == TASK_BLOCKED, "task %s in sem %s is not blocked", select->name, sem->name);
  sem->wait_list = select->wait_next;
  select->state = TASK_RUNNABLE;
  select->wait_next = NULL;
  spin_unlock(&task_lock);
}
