#ifndef OS_DEBUG_H
#define OS_DEBUG_H

#define Log(format, ...) \
    printf("\33[1;34m[%s,%d,%s] " format "\33[0m\n", \
        __FILE__,, __LINE__, __func__, ## __VA_ARGS__)

#define Assert(cond, ...) \
  do { \
    if (!(cond)) { \
      printf("\33[1;31m"); \
      printf(__VA_ARGS__); \
      printf("\33[0m\n"); \
      assert(cond); \
    } \
  } while (0)

#define os_panic(...) Assert(0, __VA_ARGS__)

#define TODO() os_panic("please implement me")

#endif
