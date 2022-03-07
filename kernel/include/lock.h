#ifndef OS_LOCK_H
#define OS_LOCK_H

typedef struct spinlock{
  int locked;
  char name[32];
  int cpu_id;
}spinlock_t;

void spin_lock(spinlock_t *lk);
void spin_unlock(spinlock_t *lk);
void spin_init(spinlock_t *lk, const char* name);
#endif
