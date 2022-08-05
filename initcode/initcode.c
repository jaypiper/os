#include <syscall.h>
/* The first program in user mode */

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

#define TESTS_BUSYBOX(f) \
f("echo \"#### independent command test\"") \
f("ash -c exit") \
f("sh -c exit") \
f("basename /aaa/bbb") \
f("cal") \
f("clear") \
f("date ") \
f("df ") \
f("dirname /aaa/bbb") \
f("dmesg ") \
f("du") \
f("expr 1 + 1") \
f("false") \
f("true") \
f("which ls") \
f("uname") \
f("uptime") \
f("printf \"abc\n\"") \
f("ps") \
f("pwd") \
f("free") \
f("hwclock") \
f("kill 10") \
f("ls") \
f("sleep 1") \
f("echo \"#### file opration test\"") \
f("touch test.txt") \
f("echo \"hello world\" > test.txt") \
f("cat test.txt") \
f("cut -c 3 test.txt") \
f("od test.txt") \
f("head test.txt") \
f("tail test.txt ") \
f("hexdump -C test.txt ") \
f("md5sum test.txt") \
f("echo \"ccccccc\" >> test.txt") \
f("echo \"bbbbbbb\" >> test.txt") \
f("echo \"aaaaaaa\" >> test.txt") \
f("echo \"2222222\" >> test.txt") \
f("echo \"1111111\" >> test.txt") \
f("echo \"bbbbbbb\" >> test.txt") \
f("sort test.txt | ./busybox uniq") \
f("stat test.txt") \
f("strings test.txt ") \
f("wc test.txt") \
f("[ -f test.txt ]") \
f("more test.txt") \
f("rm test.txt") \
f("mkdir test_dir") \
f("mv test_dir test") \
f("rmdir test") \
f("grep hello busybox_cmd.txt") \
f("cp busybox_cmd.txt busybox_cmd.bak") \
f("rm busybox_cmd.bak") \
f("find -name \"busybox_cmd.txt\"") \


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
#define OSCMP_TEST_BUSYBOX(_) {initcode_str[5], _, 0},

char* initcode_args[][5] = {
  TESTS_STATIC(OSCMP_TEST_STATIC)
};

char* busybox_cmd[][3] = {
  TESTS_BUSYBOX(OSCMP_TEST_BUSYBOX)
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
    initcode_syscall(SYS_wait4, 0, 0, 0);
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