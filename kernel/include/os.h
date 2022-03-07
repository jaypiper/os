#ifndef OS_OS_H
#define OS_OS_H
#include <kernel.h>
#include <kmt.h>
typedef struct handler_list{
  struct handler_list* next;
  int event;
  int seq;      // handler with smaller seq has higher priority
  handler_t handler;
} handler_list_t;


#endif