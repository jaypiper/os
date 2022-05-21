#include <klib.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  int num = 0;
  for(const char* _beg = s; _beg && *_beg != 0; _beg++) num ++;
  return num;
}

char *strcpy(char* dst, const char* src) {
  assert(src && dst);
  char *p1 = dst;
  const char *p2 = src;
  for( ; *p2 != 0; p1++, p2++) *p1 = *p2;
  *p1 = 0;
  return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
  assert(src && dst);
  char *p1 = dst;
  const char *p2 = src;
  size_t i;
  for(i = 0; i < n && *p2 != 0; i++, p1++, p2++) *p1 = *p2;
  for( ; i < n; i++, p1++) *p1 = 0;
  return dst;
}

char* strcat(char* dst, const char* src) {
  size_t len = strlen(dst);
  strcpy(dst + len, src);
  return dst;
}

int strcmp(const char* s1, const char* s2) { //看见一个优雅的实现，返回*p1-*p2
  const char *p1 = s1, *p2 = s2;
  for( ; *p1 != 0 && *p2 != 0; p1++, p2++){
    if(*p1 > *p2) return 1;
    else if(*p1 < *p2) return -1;
  }
  if(*p1) return 1;
  if(*p2) return -1;
  return 0;
}

int strncmp(const char* s1, const char* s2, size_t n) {
  const char *p1 = s1, *p2 = s2;
  size_t i = 0;
  for( ; *p1 != 0 && *p2 != 0 && i < n; p1++, p2++, i++){
    if(*p1 > *p2) return 1;
    else if(*p1 < *p2) return -1;
  }
  if(i == n) return 0;
  if(*p1) return 1;
  if(*p2) return -1;
  return 0;
}

void* memset(void* v,int c,size_t n) {
  int i = 0;
  for(char* beg = v; i < n; beg++, i++) *beg = c;
  return v;
}

void* memmove(void* dst, const void* src,size_t n) { 
  char buf[n+1]; //应该不会栈溢出吧
  assert(n <= 2048);
  memcpy(buf, src, n);
  memcpy(dst, buf, n);
  return dst;
}

void* memcpy(void* out, const void* in, size_t n) {
  // if(!s1 || !s2) return NULL;
  char *p1 = out;
  const char* p2 = in;
  // for(size_t i = 0; i < n && p1 && p2; p1++, p2++, i++) *p1 = *p2;
  while(n--) *p1++ = *p2++;
  return out;
}

int memcmp(const void* s1, const void* s2, size_t n) {
  assert(s1 && s2);
  const char *p1, *p2;
  size_t i;
  for(p1 = s1, p2 = s2, i = 0; i < n; p1++, p2++, i++){
    if(*p1 > *p2) return 1;
    if(*p1 < *p2) return -1;
  }
  return 0;
}

#endif
