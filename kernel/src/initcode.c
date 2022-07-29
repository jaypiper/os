#include <syscall.h>
/* The first program in user mode */

char __attribute__((section("initcode_data")))oscmp_static_str[][35] = {
"argv",
"basename",
"clocale_mbfuncs",
"clock_gettime",
"crypt",
"dirname",
"env",
"fdopen",
"fnmatch",
"fscanf",
"fwscanf",
"iconv_open",
"inet_pton",
"mbc",
"memstream",
"pthread_cancel_points",
"pthread_cancel",
"pthread_cond",
"pthread_tsd",
"qsort",
"random",
"search_hsearch",
"search_insque",
"search_lsearch",
"search_tsearch",
"setjmp",
"snprintf",
"socket",
"sscanf",
"sscanf_long",
"stat",
"strftime",
"string",
"string_memcpy",
"string_memmem",
"string_memset",
"string_strchr",
"string_strcspn",
"string_strstr",
"strptime",
"strtod",
"strtod_simple",
"strtof",
"strtol",
"strtold",
"swprintf",
"tgmath",
"time",
"tls_align",
"udiv",
"ungetc",
"utime",
"wcsstr",
"wcstol",
"pleval",
"daemon_failure",
"dn_expand_empty",
"dn_expand_ptr_0",
"fflush_exit",
"fgets_eof",
"fgetwc_buffering",
"flockfile_list",
"fpclassify_invalid_ld80",
"ftello_unflushed_append",
"getpwnam_r_crash",
"getpwnam_r_errno",
"iconv_roundtrips",
"inet_ntop_v4mapped",
"inet_pton_empty_last_field",
"iswspace_null",
"lrand48_signextend",
"lseek_large",
"malloc_0",
"mbsrtowcs_overflow",
"memmem_oob_read",
"memmem_oob",
"mkdtemp_failure",
"mkstemp_failure",
"printf_1e9_oob",
"printf_fmt_g_round",
"printf_fmt_g_zeros",
"printf_fmt_n",
"pthread_robust_detach",
"pthread_cancel_sem_wait",
"pthread_cond_smasher",
"pthread_condattr_setclock",
"pthread_exit_cancel",
"pthread_once_deadlock",
"pthread_rwlock_ebusy",
"putenv_doublefree",
"regex_backref_0",
"regex_bracket_icase",
"regex_ere_backref",
"regex_escaped_high_byte",
"regex_negated_range",
"regexec_nosub",
"rewind_clear_error",
"rlimit_open_files",
"scanf_bytes_consumed",
"scanf_match_literal_eof",
"scanf_nullbyte_char",
"setvbuf_unget",
"sigprocmask_internal",
"sscanf_eof",
"statvfs",
"strverscmp",
"syscall_sign_extend",
"uselocale_0",
"wcsncpy_read_overflow",
"wcsstr_false_negative",
};



char __attribute__((section("initcode_data")))initcode_str[][25] = {
  "in initcode\n", 
  "./runtest.exe",
  "-w",
  "entry-static.exe",
  "./hello",
};


int __attribute__((section("initcode_text")))initcode_syscall(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3){
  int ret;
  asm volatile("mv a0, %1; \
                mv a1, %2; \
                mv a2, %3; \
                mv a7, %4; \
                ecall; \
                mv %0, a0" : "=r"(ret) : "r"(val1), "r"(val2), "r"(val3), "r"((unsigned long long)syscall) : "%a0", "%a1", "%a2", "%a7");
  return ret;
}

char* __attribute__((section("initcode_data")))initcode_args[] = {
  initcode_str[1], initcode_str[2], initcode_str[3], oscmp_static_str[0], 0
};

void __attribute__((section("initcode_text")))oscmp_test_static(int i){
  int pid = initcode_syscall(SYS_clone, 17, 0, 0);
  if(pid == 0){
    initcode_syscall(SYS_execve, initcode_str[1], initcode_args, 0);
  } else{

  }

}

void __attribute__((section("initcode_entry"))) initcode_test(){
  initcode_syscall(SYS_write, 1, initcode_str[0], 12);
  // Testing busybox
  oscmp_test_static(0);
  while(1);
}