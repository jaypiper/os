#ifndef OS_LOCK_H
#define OS_LOCK_H

typedef struct lock_t{
  int locked;
  char name[32];
} lock_t;
void spin_lock(lock_t *lk);
void spin_unlock(lock_t *lk);

#endif
