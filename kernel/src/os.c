#include <common.h>
#include <os.h>

static handler_list_t* handlers_sorted;

static spinlock_t handler_lock;


static void tty_reader(void *arg) {
  device_t *tty = dev->lookup(arg);
  char cmd[128], resp[128], ps[16];
  sprintf(ps, "(%s) $ ", arg);
  while (1) {
    tty->ops->write(tty, 0, ps, strlen(ps));
    int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
    cmd[nread] = '\0';
    sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
    tty->ops->write(tty, 0, resp, strlen(resp));
  }
}

static void os_init() {
  handlers_sorted = NULL;
  spin_init(&handler_lock, "handler lock");
  pmm->init();
  kmt->init();
  dev->init();
  kmt->create(task_alloc(), "tty_reader", tty_reader, "tty1");
  kmt->create(task_alloc(), "tty_reader", tty_reader, "tty2");
}

static void os_run() {
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
#ifdef PMM_DEBUG
  extern void* pmm_test();
  pmm_test();
#endif
  iset(true);
  while (1) ;
}

Context* os_trap(Event ev, Context *context){
  Assert(ev.event != EVENT_ERROR, "recieve error event");
  Assert(ev.event != EVENT_PAGEFAULT, "recieve error event");
  Context* ret = NULL;
  mutex_lock(&handler_lock);
  for(handler_list_t* h = handlers_sorted; h; h = h->next){
    if(h->event == EVENT_NULL || h->event == ev.event){
      Context* r = h->handler(ev, context);
      Assert(!ret || !r, "returning multiple context for event %d", ev);
      if(r) ret = r;
    }
  }
  mutex_unlock(&handler_lock);
  Assert(ret, "returning NULL context for event %d", ev.event);
  // TODO: check whether s is sane
  return ret;
}

void os_on_irq(int seq, int event, handler_t handler){
  handler_list_t* new_handler = pmm->alloc(sizeof(handler_list_t));
  new_handler->seq = seq;
  new_handler->event = event;
  new_handler->handler = handler;

  mutex_lock(&handler_lock);
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
  mutex_unlock(&handler_lock);
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
  .trap = os_trap,
  .on_irq = os_on_irq,
};
