#include <common.h>
#include <os.h>
#include <user.h>

static handler_list_t* handlers_sorted;

static spinlock_t handler_lock;

static cpu_t cpus[MAX_CPU];
static spinlock_t cpu_state_lock;

cpu_t* get_cpu(){
  cpu_t* ret = &cpus[cpu_current()];
  return ret;
}
#ifndef UPROC_DEBUG
// static void tty_reader(void *arg) {
//   device_t *tty = dev->lookup(arg);
//   char cmd[128], resp[128], ps[16];
//   sprintf(ps, "(%s) $ ", arg);
//   while (1) {
//     tty->ops->write(tty, 0, ps, strlen(ps));
//     int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
//     cmd[nread] = '\0';
//     sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
//     tty->ops->write(tty, 0, resp, strlen(resp));
//   }
// }
#endif
#ifdef VFS_DEBUG
static void vfs_test(void* args){
  void filetest(int idx);
  char c = args ? ((char*)args)[0] : 0;
  if((c >= '0') && (c <= '9')) filetest(c - '0');
  else if((c >= 'a') && (c <= 'z')) filetest(c - 'a' + 10);
  else filetest(-1);
  while(1);
}
#endif

static void os_init() {
  spin_init(&cpu_state_lock, "cpu_lock");
  for(int i = 0; i < MAX_CPU; i++){
    cpus[i].intena = 1;
    cpus[i].ncli = 0;
  }
  handlers_sorted = NULL;
  spin_init(&handler_lock, "handler lock");
  pmm->init();
  kmt->init();
  dev->init();
  vfs->init();
  uproc->init();
}

static void os_test(){
#ifdef KMT_DEBUG
  void init_kmt_debug();
  init_kmt_debug();
#endif
#ifdef VFS_DEBUG
  static char c[20];
  for(int i = 0; i < 10; i++){
    c[i] = '0' + i;
    kmt->create(task_alloc(), "vfs_test", vfs_test, &c[i]);
  }
  for(int i = 0; i < 2; i++){
    c[i + 10] = 'a' + i;
    kmt->create(task_alloc(), "vfs_test", vfs_test, &c[i+10]);
  }

#endif

#ifdef UPROC_DEBUG
  void uproc_test(void* args);
  static char ids[30];
  for(int i = 0; i < 30; i ++) ids[i] = i;
  extern spinlock_t id_lock;
  kmt->spin_init(&id_lock, "id_lock");
  for(int i = 0; i < 15; i++){
    kmt->create(task_alloc(), "uproc test", uproc_test, &ids[i]);
  }
#else
  // kmt->create(task_alloc(), "tty_reader", tty_reader, "tty1");
  // kmt->create(task_alloc(), "tty_reader", tty_reader, "tty2");
  void start_initcode(void* args);
  kmt->create(task_alloc(), "uproc test", start_initcode, 0);
#endif
}

static void os_run() {
  // for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
  //   putch(*s == '*' ? '0' + cpu_current() : *s);
  // }
  iset(true);
  while (1) yield();
}

Context* os_trap(Event ev, Context *context){
  if(ev.event == EVENT_ERROR) disp_ctx(&ev, context);
  Assert(ev.event != EVENT_ERROR, "receive error event %s cause 0x%lx epc 0x%lx tval 0x%lx a0 0x%lx", ev.msg, context->cause, context->epc, ev.ref, context->gpr[10]);
  Context* ret = NULL;
  for(handler_list_t* h = handlers_sorted; h; h = h->next){
    if(h->event == EVENT_NULL || h->event == ev.event){
      Context* r = h->handler(ev, context);
      Assert(!ret || !r, "returning multiple context for event %d", ev);
      if(r) ret = r;
    }
  }
  Assert(ret, "returning NULL context for event %d", ev.event);

  return ret;
}

void os_on_irq(int seq, int event, handler_t handler){
  handler_list_t* new_handler = pmm->alloc(sizeof(handler_list_t));
  new_handler->seq = seq;
  new_handler->event = event;
  new_handler->handler = handler;

  if(!handlers_sorted || seq <= handlers_sorted->seq){
    new_handler->next = handlers_sorted;
    handlers_sorted = new_handler;
  }else{
    for(handler_list_t* prev = handlers_sorted; prev; prev = prev->next){
      if(!prev->next || seq <= prev->next->seq){
        new_handler->next = prev->next;
        prev->next = new_handler;
        break;
      }
    }
  }
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
  .trap = os_trap,
  .on_irq = os_on_irq,
  .test = os_test,
};

void assert_dummy_func(){};
