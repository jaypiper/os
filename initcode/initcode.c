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
/* f("--install", "--install", 0) \
*/f("echo \"#### independent command test\"", "echo", "#### independent command test", 0) \
f("basename /aaa/bbb", "basename", "/aaa/bbb", 0) \
f("cal", "cal", 0) \
f("clear" , "clear", 0) \
f("date" , "date", 0) \
f("df" , "df", 0) \
f("dirname /aaa/bbb", "dirname", "/aaa/bbb", 0) \
f("dmesg" , "dmesg", 0) \
f("ash -c exit", "ash", "-c", "exit", 0) \
f("sh -c exit", "sh", "-c", "exit", 0) \
f("du", "du", 0) \
f("expr 1 + 1", "expr", "1" "+" "1", 0) \
f("false", "false", 0) \
f("true", "true", 0) \
/*f("which ls", "which", "ls", 0) \
*/f("uname", "uname", 0) \
f("uptime", "uptime", 0) \
f("printf \"abc\\n\"", "printf", "abc\n", 0) \
f("ps", "ps", 0) \
f("pwd", "pwd", 0) \
f("free", "free", 0) \
/*f("hwclock", "hwclock", 0) \
*/f("kill 10", "kill", "10", 0) \
f("ls", "ls", 0) \
f("sleep 1", "sleep", "1", 0) \
f("echo \"#### file opration test\"", "echo", "#### file opration test", 0) \
f("touch test.txt", "touch", "test.txt", 0) \
f("echo \"hello world\" > test.txt", "echo", "\"hello world\"", ">", "test.txt", 0) \
f("cat test.txt", "cat", "test.txt", 0) \
f("cut -c 3 test.txt", "cut", "-c", "3", "test.txt", 0) \
f("od test.txt", "od", "test.txt", 0) \
f("head test.txt", "head", "test.txt", 0) \
f("tail test.txt" , "tail", "test.txt", 0) \
f("hexdump -C test.txt" , "hexdump", "-C", "test.txt", 0) \
f("md5sum test.txt", "md5sum", "test.txt", 0) \
f("echo \"ccccccc\" >> test.txt", "echo", "ccccccc", ">>", "test.txt", 0) \
f("echo \"2222222\" >> test.txt", "echo", "2222222", ">>", "test.txt", 0) \
f("echo \"1111111\" >> test.txt", "echo", "1111111", ">>", "test.txt", 0) \
f("echo \"bbbbbbb\" >> test.txt", "echo", "bbbbbbb", ">>", "test.txt", 0) \
f("echo \"aaaaaaa\" >> test.txt", "echo", "aaaaaaa", ">>", "test.txt", 0) \
f("echo \"bbbbbbb\" >> test.txt", "echo", "bbbbbbb", ">>", "test.txt", 0) \
f("sort test.txt | ./busybox uniq", "sort", "test.txt", "|", "./busybox uniq", 0) \
f("stat test.txt", "stat", "test.txt", 0) \
f("strings test.txt" , "strings", "test.txt", 0) \
f("wc test.txt", "wc", "test.txt", 0) \
f("[ -f test.txt ]", "[", "-f", "test.txt", "]", 0) \
f("more test.txt", "more", "test.txt", 0) \
f("rm test.txt", "rm", "test.txt", 0) \
f("mkdir test_dir", "mkdir", "test_dir", 0) \
f("mv test_dir test", "mv", "test_dir", "test", 0) \
f("rmdir test", "rmdir", "test", 0) \
f("grep hello busybox_cmd.txt", "grep", "hello", "busybox_cmd.txt", 0) \
f("cp busybox_cmd.txt busybox_cmd.bak", "cp", "busybox_cmd.txt", "busybox_cmd.bak", 0) \
f("rm busybox_cmd.bak", "rm", "busybox_cmd.bak", 0) \
f("find -name \"busybox_cmd.txt\"", "find", "-name", "busybox_cmd.txt", 0) \

#define TESTS_LUA(f) \
f("date.lua", 0) \
/*f("file_io.lua", 0) \
*/f("max_min.lua", 0) \
f("random.lua", 0) \
f("remove.lua", 0) \
f("round_num.lua", 0) \
f("sin30.lua", 0) \
f("sort.lua", 0) \
f("strings.lua", 0)

#define TEST_LMBENCH(f) \
f("./busybox", "echo", "latency", "measurements") \
f("./lmbench_all", "lat_syscall", "-P", "1", "null") \
f("./lmbench_all", "lat_syscall", "-P", "1", "read") \
f("./lmbench_all", "lat_syscall", "-P", "1", "write") \
f("./busybox", "mkdir", "-p", "/var/tmp") \
f("./busybox", "touch", "/var/tmp/lmbench") \
f("./lmbench_all", "lat_syscall", "-P", "1", "stat", "/var/tmp/lmbench") \
f("./lmbench_all", "lat_syscall", "-P", "1", "fstat", "/var/tmp/lmbench") \
f("./lmbench_all", "lat_syscall", "-P", "1", "open", "/var/tmp/lmbench") \
f("./lmbench_all", "lat_select", "-n", "100", "-P", "1", "file") \
f("./lmbench_all", "lat_sig", "-P", "1", "install") \
f("./lmbench_all", "lat_sig", "-P", "1", "catch") \
f("./lmbench_all lat_sig -P 1 prot lat_sig") \
f("./lmbench_all", "lat_pipe", "-P", "1") \
f("./lmbench_all", "lat_proc", "-P", "1", "fork") \
f("./lmbench_all", "lat_proc", "-P", "1", "exec") \
f("./busybox", "cp", "hello", "/tmp") \
f("./lmbench_all", "lat_proc", "-P", "1", "shell") \
f("./lmbench_all", "lmdd", "label=\"File /var/tmp/XXX write bandwidth:\"", "of=/var/tmp/XXX move=1m", "fsync=1", "print=3") \
f("./lmbench_all",  "lat_pagefault",  "-P", "1", "/var/tmp/XXX") \
f("./lmbench_all",  "lat_mmap",  "-P", "1", "512k", "/var/tmp/XXX") \
f("./busybox",  "echo", "file system latency") \
f("./lmbench_all", "lat_fs", "/var/tmp") \
f("./busybox",  "echo", "Bandwidth measurements") \
f("./lmbench_all",  "bw_pipe",  "-P", "1") \
f("./lmbench_all",  "bw_file_rd",  "-P", "1", "512k", "io_only", "/var/tmp/XXX") \
f("./lmbench_all",  "bw_file_rd",  "-P", "1", "512k", "open2close", "/var/tmp/XXX") \
f("./lmbench_all",  "bw_mmap_rd",  "-P", "1", "512k", "mmap_only", "/var/tmp/XXX") \
f("./lmbench_all",  "bw_mmap_rd",  "-P", "1", "512k", "open2close", "/var/tmp/XXX") \
f("./busybox",  "echo",  "context switch overhead") \
f("./lmbench_all",  "lat_ctx",  "-P", "1", "-s", "32", "2", "4", "8", "16", "24", "32", "64", "96") \

char initcode_str[][32] = {
  "in initcode\n",
  "./runtest.exe",
  "-w",
  "entry-static.exe",
  "./hello",
  "./busybox",
  "./lua"
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
#define OSCMP_TEST_BUSYBOX(_, ...) {initcode_str[5], __VA_ARGS__},
#define OSCMP_BUSYBOX_PASS(_, ...) "testcase busybox " _ " success\n",
#define OSCMP_BUSYBOX_FAIL(_, ...) "testcase busybox " _ " fail\n",
#define OSCMP_TEST_LUA(...) {initcode_str[6], __VA_ARGS__},
#define OSCMP_LUA_PASS(_, ...) "testcase lua " _ " success\n",
#define OSCMP_LUA_FAIL(_, ...) "testcase lua " _ " fail\n",
#define OSCMP_TEST_LMBENCH(...) {__VA_ARGS__},

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

char* lua_cmd[][4] = {
  TESTS_LUA(OSCMP_TEST_LUA)
};

char* lua_pass[] = {
  TESTS_LUA(OSCMP_LUA_PASS)
};

char* lua_fail[] = {
  TESTS_LUA(OSCMP_LUA_FAIL)
};

char* lmbench_cmd[][15] = {
  TEST_LMBENCH(OSCMP_TEST_LMBENCH)
};

void oscmp_test_static(int i){
  int pid = initcode_syscall(SYS_clone, 17, 0, 0);
  if(pid == 0){
    initcode_syscall(SYS_execve, initcode_str[1], initcode_args[i], 0);
  } else{
    initcode_syscall(SYS_wait4, 0, 0, 0);
  }
}

int ret = 0;
void oscmp_test_busybox(int i){
  int pid = initcode_syscall(SYS_clone, 17, 0, 0);
  if(pid == 0){
    initcode_syscall(SYS_execve, initcode_str[5], busybox_cmd[i], 0);
  } else{
    initcode_syscall(SYS_wait4, 0, &ret, 0);
    if(!ret) initcode_syscall(SYS_write, 1, busybox_pass[i], strlen(busybox_pass[i]));
    else initcode_syscall(SYS_write, 1, busybox_fail[i], strlen(busybox_fail[i]));
  }
}

void oscmp_test_lua(i){
  int pid = initcode_syscall(SYS_clone, 17, 0, 0);
  if(pid == 0){
    initcode_syscall(SYS_execve, initcode_str[6], lua_cmd[i], 0);
  } else{
    initcode_syscall(SYS_wait4, 0, &ret, 0);
    if(!ret) initcode_syscall(SYS_write, 1, lua_pass[i], strlen(lua_pass[i]));
    else initcode_syscall(SYS_write, 1, lua_fail[i], strlen(lua_fail[i]));
  }
}

void oscmp_test_lmbench(i){
  int pid = initcode_syscall(SYS_clone, 17, 0, 0);
  if(pid == 0){
    initcode_syscall(SYS_execve, lmbench_cmd[i][0], lmbench_cmd[i], 0);
  } else{
    initcode_syscall(SYS_wait4, 0, &ret, 0);
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
  // lua
  for(int i = 0; i < sizeof(lua_cmd) / sizeof(lua_cmd[0]); i++){
    oscmp_test_lua(i);
  }
  //lmbench
  for(int i = 0; i < sizeof(lmbench_cmd) / sizeof(lmbench_cmd[0]); i++){
    oscmp_test_lmbench(i);
  }
  // initcode_syscall(SYS_write, 1, "\ngoodbye\n", 8);
  while(1);
}