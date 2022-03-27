#include <common.h>

long long _sys_time(){
  AM_TIMER_UPTIME_T _timer = io_read(AM_TIMER_UPTIME);
  return _timer.us;
}

int find_replace(char* s, char* delim, int start_pos){
  s = s + start_pos;
  if(!(s || s[0])) return -1;
  int len_tok_s = strlen(s);
  for(int i = 0; i < len_tok_s; i++){
    for(int j = 0; j < strlen(delim); j++){
      if(s[i] == delim[j]){
        s[i] = 0;
        return start_pos + i;
      }
    }
  }
  return -1;
}
