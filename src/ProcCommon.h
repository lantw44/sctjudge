#ifndef SCTJUDGE_PROC_COMMON
#define SCTJUDGE_PROC_COMMON

#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <pthread.h>
#include <semaphore.h>

#include <l4darr.h>

/* --- ProcCheckTime.c --- */
extern volatile sig_atomic_t sctproc_abort;
void* sctproc_checktime(void*);

/* --- ProcMonitor.c --- */
void* sctproc_monitor(void*);
typedef struct sctjudge_procmoninfo{
	unsigned long time;
	unsigned long cpuuser;
	unsigned long cpusys;
	unsigned long vm;
} PROCMONINFO;

/* --- ProcMain.c --- */

/* 子程序 PID 和對應的 mutex */
extern pid_t           child_pid;
extern pthread_mutex_t child_mutex;

/* 兩個 thread 的資料 */
extern pthread_t       checktime_thread, monitor_thread;
extern _Bool           checktime_exist,  monitor_exist;
extern pthread_mutex_t checktime_mutex,  monitor_mutex;

/* 用來讓另外兩個 thread 卡住的 semaphore */
extern sem_t           checktime_sem, monitor_sem;

/* 判斷有無 TLE，此變數由 sctjudge_checktle 設定 */
extern _Bool           sctproc_tle;
extern pthread_mutex_t sctproc_tle_mutex;

/* 同時只能有一個函式執行輸出 */
extern pthread_mutex_t output_mutex;

/* ProcMain 作為參數和回傳的 struct，i 表示輸入，o 表示輸出 */
typedef struct sctjudge_procinfo{
	int      i_verbose;        /* quiet=0, normal=1, debug=2 */
	char*    i_exec;           /* 輸入：可執行檔的路徑 */
	L4DA*    i_argv;           /* 輸入：執行時的 argv，會取出 char** 指標 */
	char*    i_chroot;         /* 輸入：要作為 chroot 的目錄 */
	char*    i_infile;         /* 輸入：要作為 stdin 的檔案 */
	char*    i_outfile;        /* 輸入：要作為 stdout 的檔案 */
	char*    i_errfile;        /* 輸入：要作為 stderr 的檔案 */
	uid_t    i_uid;            /* 輸入：要切換成哪個使用者 */
	gid_t    i_gid;            /* 輸入：要切換成哪個群組 */
	unsigned i_flag;           /* 輸入：額外的設定值 */
	unsigned i_limit_time;     /* 輸入：執行時間限制 */
	unsigned i_limit_mem;      /* 輸入：虛擬記憶體上限    (RLIMIT_AS) */
	unsigned i_limit_core;     /* 輸入：core 檔案大小上限 (RLIMIT_CORE) */
	unsigned i_limit_cpu;      /* 輸入：CPU 時間上限      (RLIMIT_CPU) */
	unsigned i_limit_out;      /* 輸入：檔案大小上限      (RLIMIT_FSIZE) */
	unsigned i_limit_file;     /* 輸入：檔案開啟數量上限  (RLIMIT_NOFILE) */
	unsigned i_limit_proc;     /* 輸入：process 數量上限  (RLIMIT_NPROC) */
	unsigned i_limit_stack;    /* 輸入：stack 大小上限    (RLIMIT_STACK) */
	void*    i_func_arg;       /* 輸入：送入 f_ 系列函式的第一個參數 */
	int      o_result;         /* 輸出：檢測結果 */
	int      o_procexit;       /* 輸出：child process 結束的回傳值 */
	int      o_signal;         /* 輸出：受測程式因哪個 signal 結束 */
	struct timespec o_runtime; /* 輸出：受測程式執行時間 */
	struct rusage   o_rusage;  /* 輸出：child process 的 rusage */
	void   (*f_debug)    (void*, const char*, va_list);
	void   (*f_normal)   (void*, const char*, va_list);
	void   (*f_err)      (void*, const char*, va_list);
	void   (*f_monitor)  (void*, const PROCMONINFO*);
} PROCINFO;

/* PROCINFO 裡面 i_flag 的值 */
#define PROCINFO_FLAG_FORCE        0x00000001 /* 強制執行 */
#define PROCINFO_FLAG_DRYRUN       0x00000002 /* 只檢查設定而不執行 */
#define PROCINFO_FLAG_SECURE_CHECK 0x00000004 /* 檢查所有「建議的」必要選項 */
#define PROCINFO_FLAG_REDIR_STDERR 0x00000008 /* stderr 也導入輸出檔 */
#define PROCINFO_FLAG_SETUID       0x00000010 /* 要修改 uid */
#define PROCINFO_FLAG_SETGID       0x00000020 /* 要修改 gid */
#define PROCINFO_FLAG_CLOSE_FD     0x00000040 /* 不要導到/dev/null，直接關閉fd*/
#define PROCINFO_FLAG_LIMIT_MEM    0x00000080
#define PROCINFO_FLAG_LIMIT_CORE   0x00000100
#define PROCINFO_FLAG_LIMIT_CPU    0x00000200
#define PROCINFO_FLAG_LIMIT_OUT    0x00000400
#define PROCINFO_FLAG_LIMIT_FILE   0x00000800
#define PROCINFO_FLAG_LIMIT_PROC   0x00001000
#define PROCINFO_FLAG_LIMIT_STACK  0x00002000

/* PROCINFO 裡面 o_result 的值 */
#define PROCINFO_RESULT_OK  0
#define PROCINFO_RESULT_RE  1
#define PROCINFO_RESULT_TLE 2
#define PROCINFO_RESULT_OLE 3
#define PROCINFO_RESULT_SLE 4  /* 暫停次數太多 */
#define PROCINFO_RESULT_AB  5  /* 使用者中斷 */
#define PROCINFO_RESULT_UD  6  /* 未定義的東西，這應該不會出現吧 */
#define PROCINFO_RESULT_MAX 7

/* setproc_main 的回傳值 */
#define SCTJUDGE_PROC_EXIT_SUCCESS   0
#define SCTJUDGE_PROC_EXIT_INVALID   2  /* 不合理的設定值 */
#define SCTJUDGE_PROC_EXIT_INSECURE  3  /* 不安全的設定值 */
#define SCTJUDGE_PROC_EXIT_REJECT    4  /* 拒絕不符權限的操作 */
#define SCTJUDGE_PROC_EXIT_FAILED    5  /* 有些操作失敗了 */
#define SCTJUDGE_PROC_EXIT_MAX       6 

int sctproc_main(PROCINFO*);
void sctproc_setdefault(PROCINFO*);
void sctproc_freestring(PROCINFO*);
void sctproc_logger(int, const char*, ...);
void sctproc_err(const char*, ...);

#endif
