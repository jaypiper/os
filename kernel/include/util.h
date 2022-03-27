#ifndef OS_UTIL_H
#define OS_UTIL_H

long long _sys_time();
int find_replace(char* s, char* delim, int start_pos);

#define MAX(a, b) (((a) > (b))? (a) : (b))
#define MIN(a, b) (((a) > (b))? (b) : (a))

#endif
