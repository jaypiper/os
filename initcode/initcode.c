#include <syscall.h>
/* The first program in user mode */

int strlen(const char *s) {
  int num = 0;
  for(const char* _beg = s; _beg && *_beg != 0; _beg++) num ++;
  return num;
}

#define TESTS_STATIC(f) \
f("argv")    f("basename")    f("clocale_mbfuncs") f("crypt")     f("string_memcpy") \
f("fnmatch") f("fwscanf")     f("iconv_open")      f("env")       f("inet_pton") \
f("mbc")     f("memstream")   f("clock_gettime")   f("setjmp")    f("socket") \
f("dirname") f("qsort")       f("random")          f("snprintf")  f("sscanf") \
f("stat")    f("sscanf_long") f("wcstol")          f("pleval")    f("daemon_failure")\
f("string")  f("strftime")    f("string_memmem")   f("string_memset") f("string_strchr") \
f("strtof")  f("strtol")      f("strptime")        f("strtod")    f("strtod_simple") \
f("fscanf")  f("strtold")     f("swprintf")        f("tgmath")    f("tls_align") \
f("time")    f("udiv")        f("ungetc")          f("utime")     f("wcsstr") \
f("fdopen")  f("malloc_0")    f("iswspace_null")   f("lseek_large")f("lrand48_signextend") \
f("search_hsearch")  f("search_insque") f("search_lsearch")   f("search_tsearch") \
f("string_strcspn")  f("string_strstr") f("dn_expand_empty")  f("dn_expand_ptr_0") \
f("fflush_exit")     f("fgets_eof")     f("fgetwc_buffering") f("flockfile_list") \
f("fpclassify_invalid_ld80") f("ftello_unflushed_append") f("getpwnam_r_crash") \
f("pthread_cancel_points") f("pthread_cancel") f("pthread_cond") f("pthread_tsd") \
f("getpwnam_r_errno")    f("iconv_roundtrips") f("inet_ntop_v4mapped") \
f("inet_pton_empty_last_field") f("mbsrtowcs_overflow") \
f("memmem_oob_read") f("memmem_oob") f("mkdtemp_failure") f("mkstemp_failure") \
f("printf_1e9_oob") f("printf_fmt_g_round") f("printf_fmt_g_zeros") f("printf_fmt_n") \
f("pthread_robust_detach") f("pthread_cancel_sem_wait") f("pthread_cond_smasher") \
f("pthread_condattr_setclock") f("pthread_exit_cancel") f("pthread_once_deadlock") \
f("pthread_rwlock_ebusy") f("putenv_doublefree") f("regex_backref_0") \
f("regex_bracket_icase") f("regex_ere_backref") f("regex_escaped_high_byte") \
f("regex_negated_range") f("regexec_nosub") f("rewind_clear_error") \
f("rlimit_open_files") f("scanf_bytes_consumed") f("scanf_match_literal_eof") \
f("scanf_nullbyte_char") f("setvbuf_unget") f("sigprocmask_internal") f("sscanf_eof") \
f("statvfs") f("strverscmp") f("syscall_sign_extend") \
f("uselocale_0") f("wcsncpy_read_overflow") f("wcsstr_false_negative")

#define M_NARGS(...) M_NARGS_(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0)
#define M_NARGS_(_10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N

// utility (concatenation)
#define M_CONC(A, B) A##B

#define M_GET_CONCAT(N, ...) M_CONC(M_GET_CONCAT_, N)(__VA_ARGS__)
#define M_GET_CONCAT_0(_0) ""
#define M_GET_CONCAT_1(_1, _0) _1
#define M_GET_CONCAT_2(_1, _2, _0) M_GET_CONCAT_1(_1, _0) " " _2
#define M_GET_CONCAT_3(_1, _2, _3, _0) M_GET_CONCAT_2( _1, _2, _0) " " _3
#define M_GET_CONCAT_4(_1, _2, _3, _4, _0) M_GET_CONCAT_3(_1, _2, _3, _0) " " _4
#define M_GET_CONCAT_5(_1, _2, _3, _4, _5, _0) M_GET_CONCAT_4(_1, _2, _3, _4, _0) " " _5

#define TESTS_BUSYBOX(f) \
f("echo", "\"#### independent command test\"", 0) \
f("ash", "-c", "exit", 0) \
f("sh", "-c", "exit", 0) \
f("basename", "/aaa/bbb", 0) \
f("cal", 0) \
f("clear", 0) \
f("date ", 0) \
f("df ", 0) \
f("dirname", "/aaa/bbb", 0) \
f("dmesg ", 0) \
f("du", 0) \
f("expr", "1" "+" "1", 0) \
f("false", 0) \
f("true", 0) \
f("which", "ls", 0) \
f("uname", 0) \
f("uptime", 0) \
f("printf", "\"abc\n\"", 0) \
f("ps", 0) \
f("pwd", 0) \
f("free", 0) \
f("hwclock", 0) \
f("kill", "10", 0) \
f("ls", 0) \
f("sleep", "1", 0) \
f("echo", "\"#### file opration test\"", 0) \
f("touch", "test.txt", 0) \
f("echo", "\"hello world\"", ">", "test.txt", 0) \
f("cat", "test.txt", 0) \
f("cut", "-c", "3", "test.txt", 0) \
f("od", "test.txt", 0) \
f("head", "test.txt", 0) \
f("tail", "test.txt ", 0) \
f("hexdump", "-C", "test.txt ", 0) \
f("md5sum", "test.txt", 0) \
f("echo", "\"ccccccc\"", ">>", "test.txt", 0) \
f("echo", "\"bbbbbbb\"", ">>", "test.txt", 0) \
f("echo", "\"aaaaaaa\"", ">>", "test.txt", 0) \
f("echo", "\"2222222\"", ">>", "test.txt", 0) \
f("echo", "\"1111111\"", ">>", "test.txt", 0) \
f("echo", "\"bbbbbbb\"", ">>", "test.txt", 0) \
f("sort", "test.txt", "|", "./busybox uniq", 0) \
f("stat", "test.txt", 0) \
f("strings", "test.txt ", 0) \
f("wc", "test.txt", 0) \
f("[", "-f", "test.txt", "]", 0) \
f("more", "test.txt", 0) \
f("rm", "test.txt", 0) \
f("mkdir", "test_dir", 0) \
f("mv", "test_dir", "test", 0) \
f("rmdir", "test", 0) \
f("grep", "hello", "busybox_cmd.txt", 0) \
f("cp", "busybox_cmd.txt", "busybox_cmd.bak", 0) \
f("rm", "busybox_cmd.bak", 0) \
f("find", "-name", "\"busybox_cmd.txt\"", 0) \


char initcode_str[][32] = {
  "in initcode\n",
  "./runtest.exe",
  "-w",
  "entry-static.exe",
  "./hello",
  "./busybox"
};


int initcode_syscall(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3){
  int ret;
  asm volatile("mv a0, %1; \
                mv a1, %2; \
                mv a2, %3; \
                mv a7, %4; \
                ecall; \
                mv %0, a0" : "=r"(ret) : "r"(val1), "r"(val2), "r"(val3), "r"((unsigned long long)syscall) : "%a0", "%a1", "%a2", "%a7");
  return ret;
}

#define OSCMP_TEST_STATIC(_) {initcode_str[1], initcode_str[2], initcode_str[3], _, 0},
#define OSCMP_TEST_BUSYBOX(_, ...) {initcode_str[5], _, __VA_ARGS__},
#define OSCMP_BUSYBOX_PASS(...) "testcase busybox " M_GET_CONCAT(M_NARGS(__VA_ARGS__), __VA_ARGS__) " success\n",
#define OSCMP_BUSYBOX_FAIL(...) "testcase busybox " M_GET_CONCAT(M_NARGS(__VA_ARGS__), __VA_ARGS__) " fail\n",

char* initcode_args[][5] = {
  TESTS_STATIC(OSCMP_TEST_STATIC)
};

char* busybox_cmd[][7] = {
  TESTS_BUSYBOX(OSCMP_TEST_BUSYBOX)
};

char* busybox_pass[] = {
  TESTS_BUSYBOX(OSCMP_BUSYBOX_PASS)
};

char* busybox_fail[] = {
  TESTS_BUSYBOX(OSCMP_BUSYBOX_FAIL)
};

void oscmp_test_static(int i){
  int pid = initcode_syscall(SYS_clone, 17, 0, 0);
  if(pid == 0){
    initcode_syscall(SYS_execve, initcode_str[1], initcode_args[i], 0);
  } else{
    initcode_syscall(SYS_wait4, 0, 0, 0);
  }
}


void oscmp_test_busybox(int i){
  int pid = initcode_syscall(SYS_clone, 17, 0, 0);
  if(pid == 0){
    initcode_syscall(SYS_execve, initcode_str[5], busybox_cmd[i], 0);
  } else{
    int ret = 0;
    initcode_syscall(SYS_wait4, &ret, 0, 0);
    if(!ret) initcode_syscall(SYS_write, 1, busybox_pass[i], strlen(busybox_pass[i]));
    else initcode_syscall(SYS_write, 1, busybox_fail[i], strlen(busybox_fail[i]));
  }
}

void __attribute__((section("initcode_entry"))) initcode_test(){
  initcode_syscall(SYS_write, 1, initcode_str[0], 12);
  // Testing libc-test
  // for(int i = 0; i < 61; i++)
  // oscmp_test_static(i);

  // busybox
  for(int i = 0; i < sizeof(busybox_cmd) / sizeof(busybox_cmd[0]); i++){
    oscmp_test_busybox(i);
  }
  // initcode_syscall(SYS_write, 1, "\ngoodbye\n", 8);
  while(1);
}