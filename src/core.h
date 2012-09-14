#ifndef SCTJUDGE_CORE_HEADER
#define SCTJUDGE_CORE_HEADER

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <pthread.h>
#include <semaphore.h>

#define NULL_DEVICE	"/dev/null"


/* mkchild.c */

/* 子程序 PID 和對應的 mutex */
extern pid_t pidchild;
extern pthread_mutex_t pidmutex;

/* 兩個 thread 的資料 */
extern pthread_t tkill, tdisplay;
extern char tkill_yes, tdisplay_yes;
extern pthread_mutex_t tkill_mx, tdisplay_mx;

/* 用來讓另外兩個 thread 卡住的 semaphore */
extern sem_t mcthr, addthr;

/* 判斷有無 TLE，此變數由 sctjudge_checktle 設定 */
extern char judge_tle;
extern pthread_mutex_t judge_tle_mx;

/* 傳給 sctjudge_makechild 作為參數的 struct */
struct makechildopt{
	char* executable;
	char* chrootdir;
	char* inputfile;
	char* outputfile;
	int exectime;
	int memlimit;
	int outlimit;
	int flags;
	uid_t uid;
	gid_t gid;
};

/* struct makechildopt 裡面 flags 的值 */
#define SCTMC_REDIR_STDERR 0x00000001
#define SCTMC_NOCOPY       0x00000002
#define SCTMC_SETUID       0x00000004
#define SCTMC_SETGID       0x00000008
#define SCTMC_VERBOSE      0x00000010
#define SCTMC_DRYRUN       0x00000020

/* sctjudge_makechild 的回傳值，main 會接收到 */
struct makechildrval{
	int mc_exitcode;             /* 此與評測結果無關！這是 thread 本身的狀態 */
	int judge_result;            /* 就是那個兩三個英文字母的代碼 */
	int judge_exitcode;          /* 程式結束回傳值 */
	struct timespec judge_time;  /* 執行時間 */
	int judge_signal;            /* RE 時的訊號 */
	struct rusage judge_rusage;  /* 子程序結束時拿到的 rusage */
};

/* struct makechildrval 裡面 mc_exitcode 的值 */
#define SCTMCRVAL_SUCCESS   0
#define SCTMCRVAL_PREPARE   1  /* fork 之前的準備工作出錯 */
#define SCTMCRVAL_FORK      2  /* fork 失敗 */
#define SCTMCRVAL_INVALID   3  /* 子程序傳回了不正確的資料（會發生嗎？）*/
#define SCTMCRVAL_CHILDFAIL 4  /* 子程序在 exec 或之前發生錯誤無法繼續 */

/* struct makechildrval 裡面 judge_result 的值 */
#define SCTRES_OK	0
#define SCTRES_RE	1
#define SCTRES_TLE	2
#define SCTRES_OLE	3
#define SCTRES_SLE	4	/* 暫停次數太多 */
#define SCTRES_AB	5	/* 使用者中斷 */
#define SCTRES_UD	6	/* 未定義的東西，這應該不會出現吧 */
#define SCTRES_MAX	7

void* sctjudge_makechild(void*);

/* killtle.c */
extern volatile sig_atomic_t break_flag;
void* sctjudge_checktle(void*);

/* disptime.c */
void* sctjudge_dispaytime(void*);

#endif
