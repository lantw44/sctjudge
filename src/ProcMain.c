#ifdef HAVE_CONFIG_H
# include "SctConfig.h"
#endif

#include "SctVersion.h"
#include "SctConst.h"
#include "SctCommon.h"
#include "ProcCommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <pthread.h>
#include <semaphore.h>

#ifdef HAVE_CONF_CAP
#include <sys/prctl.h>
#include <sys/capability.h>
#endif

#define SCTJUDGE_PROC_CHILD_CLOSE_STDIN        0
#define SCTJUDGE_PROC_CHILD_OPEN_STDIN_SINK    1
#define SCTJUDGE_PROC_CHILD_OPEN_STDIN_FILE    2
#define SCTJUDGE_PROC_CHILD_DUP2_STDIN         3
#define SCTJUDGE_PROC_CHILD_CLOSE_STDOUT       4
#define SCTJUDGE_PROC_CHILD_OPEN_STDOUT_SINK   5
#define SCTJUDGE_PROC_CHILD_OPEN_STDOUT_FILE   6
#define SCTJUDGE_PROC_CHILD_DUP2_STDOUT        7
#define SCTJUDGE_PROC_CHILD_CLOSE_STDERR       8
#define SCTJUDGE_PROC_CHILD_REDIR_STDERR       9
#define SCTJUDGE_PROC_CHILD_OPEN_STDERR_FILE  10
#define SCTJUDGE_PROC_CHILD_DUP2_STDERR       11
#define SCTJUDGE_PROC_CHILD_CLOSE_OTHER_FDS   12
#define SCTJUDGE_PROC_CHILD_CHROOT            13
#define SCTJUDGE_PROC_CHILD_SETGID            14
#define SCTJUDGE_PROC_CHILD_SETGROUPS         15
#define SCTJUDGE_PROC_CHILD_SETUID            16
#define SCTJUDGE_PROC_CHILD_RESET_UID         17
#define SCTJUDGE_PROC_CHILD_CAP_SET_PROC      18
#define SCTJUDGE_PROC_CHILD_SETRLIMIT_MEM     19
#define SCTJUDGE_PROC_CHILD_SETRLIMIT_CORE    20
#define SCTJUDGE_PROC_CHILD_SETRLIMIT_CPU     21
#define SCTJUDGE_PROC_CHILD_SETRLIMIT_OUT     22
#define SCTJUDGE_PROC_CHILD_SETRLIMIT_FILE    23
#define SCTJUDGE_PROC_CHILD_SETRLIMIT_PROC    24
#define SCTJUDGE_PROC_CHILD_SETRLIMIT_STACK   25
#define SCTJUDGE_PROC_CHILD_EXEC              26
#define SCTJUDGE_PROC_CHILD_MAX               27


/* 變數用途在 ProcCommon.h 解釋過了 */
pid_t           child_pid;
pthread_mutex_t child_mutex;

_Bool           checktime_exist;
sem_t           checktime_sem;
pthread_t       checktime_thread;
pthread_mutex_t checktime_mutex;

_Bool           monitor_exist;
sem_t           monitor_sem;
pthread_t       monitor_thread;
pthread_mutex_t monitor_mutex;

_Bool           sctproc_tle;
pthread_mutex_t sctproc_tle_mutex;

pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 內部供 logger 和 err 使用的變數 */
static void (*sctproc_output_debug)(void*, const char*, va_list);
static void (*sctproc_output_normal)(void*, const char*, va_list);
static void (*sctproc_output_err)(void*, const char*, va_list);
static void* sctproc_output_arg;
static int sctproc_verbose;

/* 如果回傳負數表示失敗，就用這個 */
static void sctproc_childonly_fail_if_negative(int fd, int rval, int workname){
	int childonly_mesg[2];
	childonly_mesg[0] = workname;
	childonly_mesg[1] = (rval < 0) ? errno : 0;
	write(fd, &childonly_mesg, sizeof(int) * 2);
	if(rval < 0){
		exit(1);
	}
}

/* 如果回傳 NULL 表示失敗，就用這個 */
static void sctproc_childonly_fail_if_null(int fd, void* rval, int workname){
	int childonly_mesg[2];
	childonly_mesg[0] = workname;
	childonly_mesg[1] = (rval == NULL) ? errno : 0;
	write(fd, &childonly_mesg, sizeof(int) * 2);
	if(rval == NULL){
		exit(2);
	}
}

void sctproc_setdefault(PROCINFO* procinfo){
	procinfo->i_verbose = 1;
	procinfo->i_exec = NULL;
	procinfo->i_argv = NULL;
	procinfo->i_chroot = NULL;
	procinfo->i_infile = NULL;
	procinfo->i_outfile = NULL;
	procinfo->i_errfile = NULL;
	procinfo->i_uid = 65535;
	procinfo->i_gid = 65535;
	procinfo->i_limit_time = 10 * 10000;
	procinfo->i_limit_mem = 64 * 1024 * 1024;
	procinfo->i_limit_core = 0;
	procinfo->i_limit_cpu = 0;
	procinfo->i_limit_out = 0;
	procinfo->i_limit_file = 0;
	procinfo->i_limit_proc = 0;
	procinfo->i_limit_stack = 8 * 1024 * 1024;
	procinfo->i_func_arg = NULL;
	procinfo->i_flag = 
		PROCINFO_FLAG_SECURE_CHECK |
		PROCINFO_FLAG_LIMIT_FILE | 
		PROCINFO_FLAG_LIMIT_PROC |
		PROCINFO_FLAG_LIMIT_CORE;
}

void sctproc_freestring(PROCINFO* procinfo){
	int i;

	if(procinfo->i_exec != NULL){
		free(procinfo->i_exec);
		procinfo->i_exec = NULL;
	}

	if(procinfo->i_argv != NULL){
		for(i=0; i<l4da_getlen(procinfo->i_argv); i++){
			if(l4da_v(procinfo->i_argv, char*, i) != NULL){
				free(l4da_v(procinfo->i_argv, char*, i));
			}
		}
		l4da_free(procinfo->i_argv);
		procinfo->i_argv = NULL;
	}

	if(procinfo->i_chroot != NULL){
		free(procinfo->i_chroot);
		procinfo->i_chroot = NULL;
	}

	if(procinfo->i_infile != NULL){
		free(procinfo->i_infile);
		procinfo->i_infile = NULL;
	}
	if(procinfo->i_outfile != NULL){
		free(procinfo->i_outfile);
		procinfo->i_outfile = NULL;
	}
	if(procinfo->i_errfile != NULL){
		free(procinfo->i_errfile);
		procinfo->i_chroot = NULL;
	}
}

int sctproc_main(PROCINFO* procinfo){

	/* private defines */
#define private_BUFMAX 256
#define private_READLEN ((sizeof(int))*2)

	sctproc_output_debug = procinfo->f_debug;
	sctproc_output_normal = procinfo->f_normal;
	sctproc_output_err = procinfo->f_err;
	sctproc_output_arg = procinfo->i_func_arg;
	sctproc_verbose = procinfo->i_verbose;

	int i;

	int rerr;    /* 接收錯誤碼的地方 */
	char errbuf[private_BUFMAX];  /* 系統錯誤訊息緩衝區 */

	int child_pipe[2]; /* 接收 child process 狀態用的 pipe */
	int child_mesg[2]; /* child process 傳來的訊息，[0]是動作，[1]是 errno */
	int child_status;  /* wait 時回傳的 child process 狀態 */
	_Bool child_terminated;/* child process 終止了沒？ */
	char* child_workstr;   /* 指向 child process 目前狀態訊息的指標 */

	struct timespec exec_begin;
	struct timespec exec_end;

	/* 一些 child process 中專用的變數 */
	int    childonly_fdopt;
	int    childonly_fdcount;
	int    childonly_fdin;
	int    childonly_fdout;
	int    childonly_fderr;
	pid_t  childonly_pid;
	struct rlimit childonly_rlimit;
#ifdef HAVE_CONF_CAP
	cap_t  childonly_cap;
#endif

	/* 列出設定值，僅供偵錯使用 */
	sctproc_logger(2, "%s: 列出 PROCINFO 中的設定值\n", __func__);
	sctproc_logger(2, "%s: i_exec: %s\n", 
		__func__, STRING_NULL(procinfo->i_exec));
	for(i=0; l4da_v(procinfo->i_argv, char*, i) != NULL; i++){
		sctproc_logger(2, "%s: i_argv[%d]: %s\n",
			__func__, i, STRING_NULL(l4da_v(procinfo->i_argv, char*, i)));
	}
	sctproc_logger(2, "%s: i_chroot: %s\n", 
		__func__, STRING_NULL(procinfo->i_chroot));
	sctproc_logger(2, "%s: i_infile: %s\n",
		__func__, STRING_NULL(procinfo->i_infile));
	sctproc_logger(2, "%s: i_outfile: %s\n",
		__func__, STRING_NULL(procinfo->i_outfile));
	sctproc_logger(2, "%s: i_errfile: %s\n",
		__func__, STRING_NULL(procinfo->i_errfile));
	sctproc_logger(2, "%s: i_uid: %u\n", __func__, procinfo->i_uid);
	sctproc_logger(2, "%s: i_gid: %u\n", __func__, procinfo->i_gid);
	sctproc_logger(2, "%s: i_flag: FORCE: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_FORCE));
	sctproc_logger(2, "%s: i_flag: DRYRUN: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_DRYRUN));
	sctproc_logger(2, "%s: i_flag: SECURE_CHECK: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_SECURE_CHECK));
	sctproc_logger(2, "%s: i_flag: REDIR_STDERR: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_REDIR_STDERR));
	sctproc_logger(2, "%s: i_flag: SETUID: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_SETUID));
	sctproc_logger(2, "%s: i_flag: SETGID: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_SETGID));
	sctproc_logger(2, "%s: i_flag: CLOSE_FD: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_CLOSE_FD));
	sctproc_logger(2, "%s: i_flag: LIMIT_MEM: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_LIMIT_MEM));
	sctproc_logger(2, "%s: i_flag: LIMIT_CORE: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_LIMIT_CORE));
	sctproc_logger(2, "%s: i_flag: LIMIT_CPU: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_LIMIT_CPU));
	sctproc_logger(2, "%s: i_flag: LIMIT_OUT: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_LIMIT_OUT));
	sctproc_logger(2, "%s: i_flag: LIMIT_FILE: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_LIMIT_FILE));
	sctproc_logger(2, "%s: i_flag: LIMIT_PROC: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_LIMIT_PROC));
	sctproc_logger(2, "%s: i_flag: LIMIT_STACK: %s\n", __func__,
		DISPLAY_YESNO(procinfo->i_flag, PROCINFO_FLAG_LIMIT_STACK));
	sctproc_logger(2, "%s: i_limit_time: %u\n", 
		__func__, procinfo->i_limit_time);
	sctproc_logger(2, "%s: i_limit_mem: %u\n",
		__func__, procinfo->i_limit_mem);
	sctproc_logger(2, "%s: i_limit_core: %u\n",
		__func__, procinfo->i_limit_core);
	sctproc_logger(2, "%s: i_limit_cpu: %u\n",
		__func__, procinfo->i_limit_cpu);
	sctproc_logger(2, "%s: i_limit_out: %u\n",
		__func__, procinfo->i_limit_out);
	sctproc_logger(2, "%s: i_limit_file: %u\n",
		__func__, procinfo->i_limit_file);
	sctproc_logger(2, "%s: i_limit_proc: %u\n",
		__func__, procinfo->i_limit_proc);
	sctproc_logger(2, "%s: i_limit_stack: %u\n",
		__func__, procinfo->i_limit_stack);

	/* 檢查 UID == 0 的狀況 */
	sctproc_logger(2, "%s: Check for user ID setting\n", __func__);
	if(procinfo->i_uid == 0 && (procinfo->i_flag & PROCINFO_FLAG_SETUID)){
		if(procrealuid == 0){ /* 我們不用 getuid()，因為可能被 swap 過 */
			if(!(procinfo->i_flag & PROCINFO_FLAG_FORCE)){
				sctproc_err("拒絕使用 root 執行受測程式\n");
				return SCTJUDGE_PROC_EXIT_INSECURE;
			}else{
				sctproc_logger(1, 
					"警告：使用 root 身份執行受測程式，所有限制都可能被忽略\n");
			}
		}else{
			sctproc_err("非超級使用者不能把 UID 設定為 0\n");
			return SCTJUDGE_PROC_EXIT_REJECT;
		}
	}

	/* 檢查安全設定值，如果有要求的話。我們不檢查 chroot 目錄是不是空的，
	 * 因為執行檔應該會複製進去 */
	if(procinfo->i_flag & PROCINFO_FLAG_SECURE_CHECK){
		sctproc_logger(2, "%s: Check for security\n", __func__);
		if(procinfo->i_chroot == NULL){
			sctproc_err("您沒有指定 chroot 目錄\n");
			return SCTJUDGE_PROC_EXIT_INSECURE;
		}
		if(!(procinfo->i_flag & PROCINFO_FLAG_SETUID)){
			sctproc_err("您沒有修改執行受測程式時的使用者\n");
			return SCTJUDGE_PROC_EXIT_INSECURE;
		}
		if(!(procinfo->i_flag & PROCINFO_FLAG_SETGID)){
			sctproc_err("您沒有修改執行受測程式時的群組\n");
			return SCTJUDGE_PROC_EXIT_INSECURE;
		}
		if(!(procinfo->i_flag & PROCINFO_FLAG_LIMIT_MEM)){
			sctproc_err("您沒有設定記憶體使用量上限\n");
			return SCTJUDGE_PROC_EXIT_INSECURE;
		}
		if(!(procinfo->i_flag & PROCINFO_FLAG_LIMIT_CORE)){
			sctproc_err("您沒有設定 core 檔案大小上限\n");
			return SCTJUDGE_PROC_EXIT_INSECURE;
		}
		if(!(procinfo->i_flag & PROCINFO_FLAG_LIMIT_PROC)){
			sctproc_err("您沒有設定 process 數量上限\n");
			return SCTJUDGE_PROC_EXIT_INSECURE;
		}
	}
	
	/* 檢查參數，若強制執行就忽略 */
	if(!(procinfo->i_flag & PROCINFO_FLAG_FORCE)){
		sctproc_logger(2, "%s: Validate process configuration\n", __func__);
		if(procinfo->i_exec == NULL){
			sctproc_err("沒有指定受測程式可執行檔名稱\n");
			return SCTJUDGE_PROC_EXIT_INVALID;
		}
		if(procinfo->i_argv == NULL){
			sctproc_err("沒有指定執行程式時的命令列\n");
			return SCTJUDGE_PROC_EXIT_INVALID;
		}
		if(procinfo->i_outfile == NULL){
			sctproc_err("沒有指定輸出檔案\n");
			return SCTJUDGE_PROC_EXIT_INVALID;
		}
		if(procinfo->i_limit_time <= 0){
			sctproc_err("沒有指定執行時間限制\n");
			return SCTJUDGE_PROC_EXIT_INVALID;
		}
		if((procinfo->i_flag & PROCINFO_FLAG_REDIR_STDERR) && 
			(procinfo->i_errfile != NULL)){
			sctproc_err("重新導向 stderr 時，不能指定 stderr 要導向的檔案\n");
			return SCTJUDGE_PROC_EXIT_INVALID;
		}
	}


	/* 產生 child process */
	sctproc_logger(2, "%s: Creating a pipe for status reporting\n",	__func__);
	pipe(child_pipe);
	sctproc_logger(2, "%s: Creating a child process\n", __func__);
	child_pid = fork();

	if(child_pid < 0){
		rerr = errno;
		strerror_threadsafe(rerr, errbuf, private_BUFMAX);
		sctproc_logger(2, "%s: fork() = %d\n", __func__, child_pid);
		sctproc_logger(2, "%s: errno = %d (%s)\n", __func__, rerr, errbuf);
		sctproc_err("無法建立新程序：%s\n", errbuf);
		close(child_pipe[0]);
		close(child_pipe[1]);
		return SCTJUDGE_PROC_EXIT_FAILED;

	}else if(child_pid > 0){
		sctproc_logger(2, "%s: fork() = %d\n", __func__, child_pid);
		close(child_pipe[1]);

		/* 持續監測 child process 的狀態 */
		while(read_complete(child_pipe[0], (void*)child_mesg, private_READLEN)
			== private_READLEN){

			strerror_threadsafe(child_mesg[1], errbuf, private_BUFMAX);

			switch(child_mesg[0]){
				case SCTJUDGE_PROC_CHILD_CLOSE_STDIN:
					sctproc_logger(2, "%s: child: close(%d), "
						"errno = %d (%s)\n", 
						__func__, STDIN_FILENO, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "關閉標準輸入");
					break;

				case SCTJUDGE_PROC_CHILD_OPEN_STDIN_SINK:
					sctproc_logger(2, "%s: child: open(%s, O_RDONLY), "
						"errno = %d (%s)\n",
						__func__, WITH_NULL, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "開啟輸入檔案 "WITH_NULL);
					break;

				case SCTJUDGE_PROC_CHILD_OPEN_STDIN_FILE:
					sctproc_logger(2, "%s: child: open(%s, O_RDONLY), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_infile, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "開啟輸入檔案 %s",
						procinfo->i_infile);
					break;

				case SCTJUDGE_PROC_CHILD_DUP2_STDIN:
					sctproc_logger(2, "%s: child: dup2(<input file>, %d), "
						"errno = %d (%s)\n",
						__func__, STDIN_FILENO, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "重新導向標準輸入");
					break;

				case SCTJUDGE_PROC_CHILD_CLOSE_STDOUT:
					sctproc_logger(2, "%s: child: close(%d), "
						"errno = %d (%s)\n",
						__func__, STDOUT_FILENO, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "關閉標準輸出");
					break;

				case SCTJUDGE_PROC_CHILD_OPEN_STDOUT_SINK:
					sctproc_logger(2, "%s: child: open(%s, "
						"O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR), "
						"errno = %d (%s)\n",
						__func__, WITH_NULL, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "開啟輸出檔案"WITH_NULL);
					break;

				case SCTJUDGE_PROC_CHILD_OPEN_STDOUT_FILE:
					sctproc_logger(2, "%s: child: open(%s, "
						"O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_outfile, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "開啟輸出檔案 %s",
						procinfo->i_outfile);
					break;

				case SCTJUDGE_PROC_CHILD_DUP2_STDOUT:
					sctproc_logger(2, "%s: child: dup2(<output file>, %d), "
						"errno = %d (%s)\n",
						__func__, STDOUT_FILENO, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "重新導向標準輸出");
					break;

				case SCTJUDGE_PROC_CHILD_CLOSE_STDERR:
					sctproc_logger(2, "%s: child: close(%d), "
						"errno = %d (%s)\n",
						__func__, STDERR_FILENO, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "關閉標準錯誤");
					break;

				case SCTJUDGE_PROC_CHILD_REDIR_STDERR:
					sctproc_logger(2, "%s: child: dup2(<output file>, %d), "
						"errno = %d (%s)\n",
						__func__, STDERR_FILENO, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "將標準輸出導向標準錯誤");
					break;

				case SCTJUDGE_PROC_CHILD_OPEN_STDERR_FILE:
					sctproc_logger(2, "%s: child: open(%s, "
						"O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_errfile, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "開啟錯誤檔案 %s",
						procinfo->i_errfile);
					break;

				case SCTJUDGE_PROC_CHILD_DUP2_STDERR:
					sctproc_logger(2, "%s: child: dup2(<err file>, %d), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_errfile, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "重新導向標準錯誤");
					break;

				case SCTJUDGE_PROC_CHILD_CLOSE_OTHER_FDS:
					sctproc_logger(2, "%s: child: for(i=3; i<getdtablesize();"
						" i++){ close(i); }, errno = %d (%s)\n", 
						__func__, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "關閉所有其他已開啟的檔案");
					break;

				case SCTJUDGE_PROC_CHILD_CHROOT:
					sctproc_logger(2, "%s: child: chroot(\"%s\"), "
						"errno = %d (%s)\n", 
						__func__, procinfo->i_chroot, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "切換根目錄至 %s", 
						procinfo->i_chroot);
					break;

				case SCTJUDGE_PROC_CHILD_SETGID:
					sctproc_logger(2, "%s: child: setgid(%u), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_gid, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定 GID 為 %u",
						procinfo->i_gid);
					break;

				case SCTJUDGE_PROC_CHILD_SETGROUPS:
					sctproc_logger(2, "%s: child: setgroups(1, { %u }), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_gid, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定 supplementary GID 為 "
						"%u", procinfo->i_gid);
					break;

				case SCTJUDGE_PROC_CHILD_SETUID:
				case SCTJUDGE_PROC_CHILD_RESET_UID:
					sctproc_logger(2, "%s: child: setuid(%u), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_uid, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定 UID 為 %d\n", 
						procinfo->i_uid);
					break;

				case SCTJUDGE_PROC_CHILD_CAP_SET_PROC:
					sctproc_logger(2, "%s: child: cap_set_proc(), "
						"errno = %d (%s)\n",
						__func__, child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "Drop Linux capabilites");
					break;

				case SCTJUDGE_PROC_CHILD_SETRLIMIT_MEM:
					sctproc_logger(2, "%s: child: setrlimit(RLIMIT_AS, "
						"{ %u, %u }), errno = %d (%s)\n",
						__func__, procinfo->i_limit_mem, procinfo->i_limit_mem,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定記憶體用量限制");
					break;

				case SCTJUDGE_PROC_CHILD_SETRLIMIT_CORE:
					sctproc_logger(2, "%s: child: setrlimit(RLIMIT_CORE, "
						"{ %u, %u }), errno = %d (%s)\n",
						__func__, procinfo->i_limit_core, 
						procinfo->i_limit_core,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定 core dump 大小限制");
					break;

				case SCTJUDGE_PROC_CHILD_SETRLIMIT_CPU:
					sctproc_logger(2, "%s: child: setrlimit(RLIMIT_CPU, "
						"{ %u, %u }), errno = %d (%s)\n",
						__func__, procinfo->i_limit_cpu, procinfo->i_limit_cpu,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定 CPU 時間限制");
					break;

				case SCTJUDGE_PROC_CHILD_SETRLIMIT_OUT:
					sctproc_logger(2, "%s: child: setrlimit(RLIMIT_FSIZE, "
						"{ %u, %u }), errno = %d (%s)\n",
						__func__, procinfo->i_limit_out, procinfo->i_limit_out,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定輸出大小限制");
					break;

				case SCTJUDGE_PROC_CHILD_SETRLIMIT_FILE:
					sctproc_logger(2, "%s: child: setrlimit(RLIMIT_NOFILE, "
						"{ %u, %u }), errno = %d (%s)\n",
						__func__, procinfo->i_limit_file, 
						procinfo->i_limit_file,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定檔案開啟數量限制");
					break;

				case SCTJUDGE_PROC_CHILD_SETRLIMIT_PROC:
					sctproc_logger(2, "%s: child: setrlimit(RLIMIT_NPROC, "
						"{ %u, %u }), errno = %d (%s)\n",
						__func__, procinfo->i_limit_proc, 
						procinfo->i_limit_proc,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定程序數量限制");
					break;

				case SCTJUDGE_PROC_CHILD_SETRLIMIT_STACK:
					sctproc_logger(2, "%s: child: setrlimit(RLIMIT_STACK, "
						"{ %u, %u }), errno = %d (%s)\n",
						__func__, procinfo->i_limit_stack, 
						procinfo->i_limit_stack,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "設定 stack 大小限制");
					break;

				case SCTJUDGE_PROC_CHILD_EXEC:
					sctproc_logger(2, "%s: child: execv(\"%s\", ...), "
						"errno = %d (%s)\n",
						__func__, procinfo->i_exec, procinfo->i_exec,
						child_mesg[1], errbuf);
					sprintf_malloc(&child_workstr, "執行受測程式 %s", 
						procinfo->i_exec);
					break;

				default:
					sctproc_logger(2, "%s: child: unknown message\n", __func__);
					sprintf_malloc(&child_workstr, "不明的訊息");
			}

			if(child_mesg[1]){
				sctproc_err("子程序傳回錯誤：%s：%s\n", child_workstr, errbuf);
				free(child_workstr);
				sctproc_logger(2, "%s: Kill the child process NOW!", __func__);
				kill(child_pid, SIGKILL);
				sctproc_logger(2, "%s: Wait for the child process", __func__);
				waitpid(child_pid, NULL, 0);
				close(child_pipe[0]);
				return SCTJUDGE_PROC_EXIT_FAILED;

			}else{
				free(child_workstr);
			}
		}

		/* exec 成功，正式啟動所有 judge 所需的東西 */
		sctproc_logger(2, "%s: 受測程式已成功執行\n", __func__);
		clock_gettime(CLOCK_REALTIME, &exec_begin);
		sctproc_logger(2, "%s: 現在時間是 (timespec) %d.%ld\n", __func__, 
			exec_begin.tv_sec, exec_begin.tv_nsec);
		close(child_pipe[0]);

		/* 初始化所有同步用的東西 */
		sctproc_logger(2, "%s: Initialize mutexes and semaphores\n", __func__);
		pthread_mutex_init(&checktime_mutex, NULL);
		pthread_mutex_init(&sctproc_tle_mutex, NULL);
		sem_init(&checktime_sem, 0, 0);
		if(procinfo->i_verbose > 0){
			pthread_mutex_init(&monitor_mutex, NULL);
			sem_init(&monitor_sem, 0, 0);
		}

		/* 產生 thread，一樣要有 verbose > 0 才有 monitor */
		sctproc_logger(2, "%s: Create thread: checktime\n", __func__);
		pthread_create(&checktime_thread, NULL, &sctproc_checktime, procinfo);
		pthread_mutex_lock(&checktime_mutex);
		checktime_exist = 1;
		pthread_mutex_unlock(&checktime_mutex);
		if(procinfo->i_verbose > 0){
			sctproc_logger(2, "%s: Create thread: monitor\n", __func__);
			pthread_create(&monitor_thread, NULL, &sctproc_monitor, procinfo);
			pthread_mutex_lock(&monitor_mutex);
			monitor_exist = 1;
			pthread_mutex_unlock(&monitor_mutex);
		}
		
		/* 開始要 wait 了，莫名其妙 stop 的程式我最多只會送
		 * SCTJUDGE_PROC_MAX_STOP_TIMES 次 SIGCONT 給你（避免進入無限迴圈）
		 * 這種奇怪的程式就等著 SLE 吧 
		 * （這個常數定義在 SctConst.h，我想這不是很重要的東西）*/
		for(i=0, child_terminated=0; i<= SCTJUDGE_PROC_MAX_STOP_TIMES; i++){
			wait4(child_pid, &child_status, WUNTRACED, &procinfo->o_rusage);
			if(WIFSTOPPED(child_status) && i!=SCTJUDGE_PROC_MAX_STOP_TIMES){
				sctproc_logger(1, "受測程式因訊號 %d 而暫停，自動傳送 SIGCONT "
					"(第 %d 次，最多可傳 %d 次)\n",	WSTOPSIG(child_status),
				   	i+1, SCTJUDGE_PROC_MAX_STOP_TIMES);
				kill(child_pid, SIGCONT);
			}else{
				if(WIFSIGNALED(child_status)){
					clock_gettime(CLOCK_REALTIME, &exec_end);
					child_terminated = 1;
					procinfo->o_signal = WTERMSIG(child_status);
					if(procinfo->o_signal == SIGXFSZ){
						procinfo->o_result = PROCINFO_RESULT_OLE;
					}else{
						procinfo->o_result = PROCINFO_RESULT_RE;
					}
					procinfo->o_procexit = WEXITSTATUS(child_status);
					break;
				}
				if(WIFEXITED(child_status)){
					clock_gettime(CLOCK_REALTIME, &exec_end);
					child_terminated = 1;
					procinfo->o_result = PROCINFO_RESULT_OK;
					procinfo->o_procexit = WEXITSTATUS(child_status);
					break;
				}
				sctproc_logger(2, "%s: The child process is terminated\n",
					__func__);
			}
		}

		/* 暫停次數超過限制 */
		if(!child_terminated){
			sctproc_logger(2, "%s: Sending SIGKILL\n", __func__);
			kill(child_pid, SIGKILL);
			wait4(child_pid, &child_status, WUNTRACED, &procinfo->o_rusage);
			procinfo->o_result = PROCINFO_RESULT_SLE;
			procinfo->o_procexit = WEXITSTATUS(child_status);
			clock_gettime(CLOCK_REALTIME, &exec_end);
		}


		/* 通知兩個 thread，請他們結束，也等他們結束 */
		pthread_mutex_lock(&checktime_mutex);
		if(checktime_exist){
			checktime_exist = 0;
			pthread_mutex_unlock(&checktime_mutex);
			sctproc_logger(2, "%s: Wait for thread checktime to exit\n",
				__func__);
			sem_post(&checktime_sem);
			pthread_join(checktime_thread, NULL);
		}else{
			pthread_mutex_unlock(&checktime_mutex);
		}

		pthread_mutex_lock(&monitor_mutex);
		if(monitor_exist){
			monitor_exist = 0;
			pthread_mutex_unlock(&monitor_mutex);
			sctproc_logger(2, "%s: Wait for thread monitor to exit\n",
				__func__);
			sem_post(&monitor_sem);
			pthread_join(monitor_thread, NULL);
		}else{
			pthread_mutex_unlock(&monitor_mutex);
		}

		/* 判斷 TLE 和執行時間 */
		difftimespec(&exec_begin, &exec_end, &(procinfo->o_runtime));
		checktimespec(&(procinfo->o_runtime));

		pthread_mutex_lock(&sctproc_tle_mutex);
		if(sctproc_tle){
			procinfo->o_result = PROCINFO_RESULT_TLE;
		}
		pthread_mutex_unlock(&sctproc_tle_mutex);

		if(sctproc_abort){
			procinfo->o_result = PROCINFO_RESULT_AB;
		}

		/* 移除掉所有同步用的東西 */
		sctproc_logger(2, "%s: Destroy mutexes and semaphores\n", __func__);
		pthread_mutex_destroy(&child_mutex);
		pthread_mutex_destroy(&checktime_mutex);
		pthread_mutex_destroy(&monitor_mutex);
		pthread_mutex_destroy(&sctproc_tle_mutex);
		sem_destroy(&checktime_sem);
		sem_destroy(&monitor_sem);

	}else{ 
		/* 這裡是 child process，也是要用到 setuid 的地方 
		 * 因為我們在 main() 就已經停用 setuid 了
		 * 所以我們必須在這段中需要的地方啟用他
		 * 如果是用 Linux capabilites 舊部需要做而外處理
		 * 如 */

		close(child_pipe[0]);

		/* close-on-exec 用來關閉這個暫時的資料傳輸通道 */
		childonly_fdopt = fcntl(child_pipe[1], F_GETFD);
		fcntl(child_pipe[1], F_SETFD, childonly_fdopt | FD_CLOEXEC);

		/* 設為獨立的 process group，不過失敗就算了，這不重要 */
		childonly_pid = getpid();
		setpgid(childonly_pid, childonly_pid);

		/* 重設所有 signal handler
		 * XXX 我猜 signal 最大值是 31，還有其他更好的寫法嗎？ */
		for(i=1; i<=31; i++){
			signal(i, SIG_DFL);
		}


		/* 開啟或關閉輸入檔案 */
		if(procinfo->i_infile == NULL &&
			(procinfo->i_flag & PROCINFO_FLAG_CLOSE_FD)){
			sctproc_childonly_fail_if_negative(child_pipe[1],
				close(STDIN_FILENO),
				SCTJUDGE_PROC_CHILD_CLOSE_STDIN);
		}else{
			if(procinfo->i_infile == NULL){
				sctproc_childonly_fail_if_negative(child_pipe[1],
					childonly_fdin = open(WITH_NULL, O_RDONLY),
					SCTJUDGE_PROC_CHILD_OPEN_STDIN_SINK);
			}else{
				sctproc_childonly_fail_if_negative(child_pipe[1],
					childonly_fdin = open(procinfo->i_infile, O_RDONLY),
					SCTJUDGE_PROC_CHILD_OPEN_STDIN_FILE);
			}
			sctproc_childonly_fail_if_negative(child_pipe[1],
				dup2(childonly_fdin, STDIN_FILENO),
				SCTJUDGE_PROC_CHILD_DUP2_STDIN);
		}

		/* 開啟或關閉輸出檔案 */
		childonly_fdout = -1;
		if(procinfo->i_outfile == NULL &&
			(procinfo->i_flag & PROCINFO_FLAG_CLOSE_FD)){
			sctproc_childonly_fail_if_negative(child_pipe[1],
				close(STDOUT_FILENO),
				SCTJUDGE_PROC_CHILD_CLOSE_STDOUT);
		}else{
			if(procinfo->i_outfile == NULL){
				sctproc_childonly_fail_if_negative(child_pipe[1],
					childonly_fdout = open(WITH_NULL, 
						O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR),
					SCTJUDGE_PROC_CHILD_OPEN_STDOUT_SINK);
			}else{
				sctproc_childonly_fail_if_negative(child_pipe[1],
					childonly_fdout = open(procinfo->i_outfile,
						O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR),
					SCTJUDGE_PROC_CHILD_OPEN_STDOUT_FILE);
			}
			sctproc_childonly_fail_if_negative(child_pipe[1],
				dup2(childonly_fdout, STDOUT_FILENO),
				SCTJUDGE_PROC_CHILD_DUP2_STDOUT);
		}

		/* 開啟或關閉標準錯誤檔案 */
		if(procinfo->i_errfile == NULL){
			if(procinfo->i_flag & PROCINFO_FLAG_CLOSE_FD){
				sctproc_childonly_fail_if_negative(child_pipe[1],
					close(STDERR_FILENO),
					SCTJUDGE_PROC_CHILD_CLOSE_STDERR);
			}
			if(procinfo->i_flag & PROCINFO_FLAG_REDIR_STDERR && 
					childonly_fdout >= 0){
				sctproc_childonly_fail_if_negative(child_pipe[1],
					dup2(childonly_fdout, STDERR_FILENO),
					SCTJUDGE_PROC_CHILD_REDIR_STDERR);
			}
		}else{
			sctproc_childonly_fail_if_negative(child_pipe[1],
				childonly_fderr = open(procinfo->i_errfile,
					O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR),
				SCTJUDGE_PROC_CHILD_OPEN_STDERR_FILE);
			sctproc_childonly_fail_if_negative(child_pipe[1],
				dup2(childonly_fderr, STDERR_FILENO),
				SCTJUDGE_PROC_CHILD_DUP2_STDERR);
		}

		/* 很暴力地把所有不該留著的 fd 關掉 */
		childonly_fdcount = getdtablesize();
		for(i=3; i<childonly_fdcount; i++){
			close(i);
		}
		child_mesg[0] = SCTJUDGE_PROC_CHILD_CLOSE_OTHER_FDS;
		child_mesg[1] = 0;
		write(child_pipe[1], &child_mesg, sizeof(int) * 2);

		/* 接下來的操作可能需要提升的權限 */
#ifndef HAVE_CONF_CAP
		enable_setuid();
#endif

		/* 進行 chroot */
		if(procinfo->i_chroot != NULL){
			sctproc_childonly_fail_if_negative(child_pipe[1],
				chroot(procinfo->i_chroot),
				SCTJUDGE_PROC_CHILD_CHROOT);
			chdir("/");
		}

#ifdef HAVE_CONF_CAP
		/* 確保修改身份的時候 capabilities 仍然留著
		 * 等一下再自己把 capabilities 丟掉 */
		prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
#endif		

		/* 我要 setgid 嗎？ */
		if(procinfo->i_flag & PROCINFO_FLAG_SETGID){
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setgid(procinfo->i_gid),
				SCTJUDGE_PROC_CHILD_SETGID);
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setgroups(1, &(procinfo->i_gid)),
				SCTJUDGE_PROC_CHILD_SETGROUPS);
		}
		
		/* 我要 setuid 嗎？ */
		if(procinfo->i_flag & PROCINFO_FLAG_SETGID){
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setuid(procinfo->i_uid),
				SCTJUDGE_PROC_CHILD_SETUID);
		}
		else{
			/* 即使不要求 setuid，也不能讓受測程式拿到 setuid root */
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setuid(procrealuid),
				SCTJUDGE_PROC_CHILD_RESET_UID);
		}


#ifdef HAVE_CONF_CAP
		/* 丟掉所有 Linux capabilities */
		childonly_cap = cap_init();
		cap_clear_flag(childonly_cap, CAP_EFFECTIVE);
		cap_clear_flag(childonly_cap, CAP_INHERITABLE);
		cap_clear_flag(childonly_cap, CAP_PERMITTED);
		sctproc_childonly_fail_if_negative(child_pipe[1],
			cap_set_proc(childonly_cap),
			SCTJUDGE_PROC_CHILD_CAP_SET_PROC);
		/* 需要 drop bound 嗎？再考慮看看 */
#endif

		/* 至此所有特殊權力都已丟棄 */

		
		/* 開始設定資源限制 */

		/* -- MEM -- */
		if(procinfo->i_flag & PROCINFO_FLAG_LIMIT_MEM){
			childonly_rlimit.rlim_cur = procinfo->i_limit_mem;
			childonly_rlimit.rlim_max = procinfo->i_limit_mem;
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setrlimit(RLIMIT_AS, &childonly_rlimit),
				SCTJUDGE_PROC_CHILD_SETRLIMIT_MEM);
		}

		/* -- CORE -- */
		if(procinfo->i_flag & PROCINFO_FLAG_LIMIT_CORE){
			childonly_rlimit.rlim_cur = procinfo->i_limit_core;
			childonly_rlimit.rlim_max = procinfo->i_limit_core;
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setrlimit(RLIMIT_CORE, &childonly_rlimit),
				SCTJUDGE_PROC_CHILD_SETRLIMIT_CORE);
		}

		/* -- CPU -- */
		if(procinfo->i_flag & PROCINFO_FLAG_LIMIT_CPU){
			childonly_rlimit.rlim_cur = procinfo->i_limit_cpu;
			childonly_rlimit.rlim_max = procinfo->i_limit_cpu;
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setrlimit(RLIMIT_CPU, &childonly_rlimit),
				SCTJUDGE_PROC_CHILD_SETRLIMIT_CPU);
		}

		/* -- OUT -- */
		if(procinfo->i_flag & PROCINFO_FLAG_LIMIT_OUT){
			childonly_rlimit.rlim_cur = procinfo->i_limit_out;
			childonly_rlimit.rlim_max = procinfo->i_limit_out;
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setrlimit(RLIMIT_FSIZE, &childonly_rlimit),
				SCTJUDGE_PROC_CHILD_SETRLIMIT_FILE);
		}

		/* -- FILE -- */
		if(procinfo->i_flag & PROCINFO_FLAG_LIMIT_FILE){
			childonly_rlimit.rlim_cur = procinfo->i_limit_file;
			childonly_rlimit.rlim_max = procinfo->i_limit_file;
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setrlimit(
#ifdef RLIMIT_NOFILE
					RLIMIT_NOFILE
#else
					RLIMIT_OFILE
#endif
					, &childonly_rlimit),
				SCTJUDGE_PROC_CHILD_SETRLIMIT_FILE);
		}

		/* -- PROC -- */
		if(procinfo->i_flag & PROCINFO_FLAG_LIMIT_PROC){
			childonly_rlimit.rlim_cur = procinfo->i_limit_proc;
			childonly_rlimit.rlim_max = procinfo->i_limit_proc;
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setrlimit(RLIMIT_NPROC, &childonly_rlimit),
				SCTJUDGE_PROC_CHILD_SETRLIMIT_PROC);
		}

		/* -- STACK -- */
		if(procinfo->i_flag & PROCINFO_FLAG_LIMIT_STACK){
			childonly_rlimit.rlim_cur = procinfo->i_limit_stack;
			childonly_rlimit.rlim_max = procinfo->i_limit_stack;
			sctproc_childonly_fail_if_negative(child_pipe[1],
				setrlimit(RLIMIT_STACK, &childonly_rlimit),
				SCTJUDGE_PROC_CHILD_SETRLIMIT_STACK);
		}
	
		/* 一切都完成了，現在要 exec 了！ */
		sctproc_childonly_fail_if_negative(child_pipe[1],
			execv(procinfo->i_exec, l4da_data(procinfo->i_argv)),
			SCTJUDGE_PROC_CHILD_EXEC);
	}


	return SCTJUDGE_PROC_EXIT_SUCCESS;

	/* drop private defines */
#undef private_BUFMAX
#undef private_READLEN

}

void sctproc_logger(int level, const char* format, ...){
	va_list ap;
	va_start(ap, format);
	if(sctproc_verbose >= level){
		pthread_mutex_lock(&output_mutex);
		if(level >= 2){
			(*sctproc_output_debug)(sctproc_output_arg, format, ap);
		}else{
			(*sctproc_output_normal)(sctproc_output_arg, format, ap);
		}
		pthread_mutex_unlock(&output_mutex);
	}
	va_end(ap);
}

void sctproc_err(const char* format, ...){
	va_list ap;
	va_start(ap, format);
	pthread_mutex_lock(&output_mutex);
	(*sctproc_output_err)(sctproc_output_arg, format, ap);
	pthread_mutex_unlock(&output_mutex);
	va_end(ap);
}

#if 0
static const char* childmsg_text[SCTCHILD_MSGMAX] = {
	"開啟輸入檔案",
	"重新導向標準輸入",
	"開啟輸出檔案",
	"重新導向標準輸出",
	"開啟用於導向標準錯誤的檔案",
	"重新導向標準錯誤",
	"關閉所有不使用的檔案",
	"設定記憶體限制",
	"設定輸出限制",
	"設定禁止開啟其他檔案",
	"設定禁止產生新程序",
	"執行 chroot",
	"修改 real 和 effective UID",
	"修改 real 和 effective GID",
	"修改 supplementary GIDs",
	"丟棄所有 Linux capabilities",
	"執行受測程式"
};

static void sctjudge_makechild_cleanup_p1(void){
	pthread_mutex_lock(&tkill_mx);
	if(tkill_yes){
		tkill_yes = 0;
		pthread_mutex_unlock(&tkill_mx);
		sem_post(&tlethr);
	}else{
		pthread_mutex_unlock(&tkill_mx);
	}

	pthread_mutex_lock(&tdisplay_mx);
	if(tdisplay_yes){
		tdisplay_yes = 0;
		pthread_mutex_unlock(&tdisplay_mx);
		sem_post(&dispthr);
	}else{
		pthread_mutex_unlock(&tdisplay_mx);
	}

}
	
static void sctjudge_makechild_cleanup_p2(mcopt, oldperm, copiedexe)
	const struct makechildopt* mcopt;
	const mode_t oldperm;
	char* copiedexe;
{
	if(oldperm != -1 && mcopt->chrootdir != NULL){
		if(mcopt->flags & SCTMC_VERBOSE){
			printf("恢復 %s 的權限\n", mcopt->chrootdir);
		}
		chmod(mcopt->chrootdir, oldperm);
	}
	if(copiedexe != NULL && mcopt->chrootdir != NULL && 
			!(mcopt->flags & SCTMC_NOCOPY)){
		if(mcopt->flags & SCTMC_VERBOSE){
			printf("移除 %s......\n", copiedexe);
		}
		unlink(copiedexe);
		free(copiedexe);
	}
}

static void sctjudge_makechild_cleanup(mcopt, oldperm, copiedexe)
	const struct makechildopt* mcopt;
	const mode_t oldperm;
	char* copiedexe;
{
	sctjudge_makechild_cleanup_p1();
	sctjudge_makechild_cleanup_p2(mcopt, oldperm, copiedexe);
}

static int copyfilebyname(const char* src, const char* destdir
		, char** srcsn, char** destfn){
	/* strerror 並不 thread safe，因此必須確定只有一個 thread 會用到 */
	/* 為了確保安全，要求目的地目錄必須是空的 */
	DIR* dirp;
	if((dirp=opendir(destdir)) == NULL){
		fprintf(stderr, "無法開啟目錄 %s：%s\n", destdir, strerror(errno));
		return -1;
	}
	struct dirent* entryp;
	while((entryp=readdir(dirp)) != NULL){
		if(entryp->d_name[0] != '.' || 
			(entryp->d_name[1] != '.' && entryp->d_name[1] != '\0')){
			fprintf(stderr, "拒絕複製檔案：目錄 %s 不是空的\n", destdir);
			return -1;
		}
	}
	int fd, fd2;
	if((fd=open(src, O_RDONLY)) < 0){
		fprintf(stderr, "無法讀取檔案 %s：%s\n", src, strerror(errno));
		return -1;
	}
	const int bufsize = 4096;
	void* buf = malloc(bufsize);
	char* srcshortname;
	char* tmp;
	if((tmp=strrchr(src, '/')) == NULL){
		srcshortname = (char*)src;
	}else{
		srcshortname = tmp + 1;
	}
	*srcsn = srcshortname;
	char* destfilename = (char*)
		malloc(strlen(srcshortname) + strlen(destdir) + 1);
	sprintf(destfilename, "%s/%s", destdir, srcshortname);
	*destfn = destfilename;
	/* 新檔案權限是 0755，umask 亂弄的我可不管 */
	if((fd2=open(destfilename, O_CREAT | O_WRONLY | O_EXCL, 
				S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) < 0){
		fprintf(stderr, "無法建立檔案 %s：%s\n", 
				destfilename, strerror(errno));
		free(buf);
		return -1;
	}
	int readcount;
	while((readcount = read(fd, buf, bufsize)) > 0){
		write(fd2, buf, readcount);
	}
	close(fd);
	close(fd2);
	free(buf);
	return 0;
}

static void sctjudge_list_settings(const struct makechildopt* mcopt){
	printf("\t受測程式可執行檔名稱：%s", mcopt->executable);
	if(mcopt->flags & SCTMC_NOCOPY && mcopt->chrootdir != NULL){
		printf(" (相對於 %s)\n"
			"\t受測程式執行時的根目錄：%s\n"
			"\t執行前複製檔案：否\n"
			, mcopt->chrootdir, mcopt->chrootdir);
	}else{
		putchar('\n');
		if(mcopt->chrootdir != NULL){
			printf("\t受測程式執行時的根目錄：%s\n"
				"\t執行前複製檔案：是\n"
				, mcopt->chrootdir);
		}else{
			puts("\t受測程式執行時的根目錄：/\n"
			"\t執行前複製檔案：否");
		}
	}
	printf("\t輸入檔案：%s\n"
		"\t輸出檔案：%s"
		, mcopt->inputfile, mcopt->outputfile);
	if(mcopt->flags & SCTMC_REDIR_STDERR){
		puts(" (重新導向標準輸出和標準錯誤)");
	}else{
		puts(" (重新導向標準輸出)");
	}
	printf("\t執行時間限制：%d 毫秒\n", mcopt->exectime);
	fputs("\t記憶體使用量限制：", stdout);
	if(mcopt->memlimit > 0){
		printf("%d 位元組\n", mcopt->memlimit);
	}else{
		puts("無限制");
	}
	fputs("\t輸出限制：", stdout);
	if(mcopt->outlimit > 0){
		printf("%d 位元組\n", mcopt->outlimit);
	}else{
		puts("無限制");
	}
	if(mcopt->flags & SCTMC_SETUID){
		printf("\t設定受測程式執行時的 UID 為 %u\n", mcopt->uid);
	}else{
		puts("\t執行受測程式時維持原有的 real UID 和 effective UID");
	}
	if(mcopt->flags & SCTMC_SETGID){
		printf("\t設定受測程式執行時的 GID 為 %u\n", mcopt->gid);
	}else{
		puts("\t執行受測程式時維持原有的 real GID、effective GID "
				"和 supplementary GID");
	}
}

static int read_size(int fd, void* buf, size_t count){
	/* 一定要讀到 count 位元組的資料才會回傳，所以只要回傳 < count
	 * 就表示已經讀到 EOF 了 */
	char* bufp = (char*)buf;
	size_t curcount = 0;
	int rval;
	do{
		rval = read(fd, bufp, count - curcount);
		if(rval < 0){
			return rval;
		}else if(rval == 0){
			return curcount;
		}else{
			bufp += rval;
			curcount += rval;
		}
	}while(curcount < count);
	return count;
}

void* sctjudge_makechild(void* arg){
	/* 首先等所有 thread 都啟動完成再開始動作 */
	sem_wait(&mcthr);

	/* 宣告變數囉！ */
	int i;

	/* 函式參數與回傳值，Judge 結果就當作回傳值 */
	struct makechildopt mcopt = *(struct makechildopt*)arg;
	struct makechildrval* toreturn = 
		(struct makechildrval*)malloc(sizeof(struct makechildrval));
	memset(toreturn, 0, sizeof(struct makechildrval));

	/* 可執行檔名稱儲存區 */
	char* execshortname = NULL; /* 可以說就是 basename，chroot+copy 才會用 */
   	char* execdestname = NULL; /* 複製出來的暫時執行檔，chroot+copy 才會用 */
	/* 這幾個變數用來儲存 Judge 結果，如果沒有啟用 verbose 
	 * 且一切正常，那這些是唯一會輸出的東西了，目的是輸出容
	 * 易被其他程式分析 */

	/* 要 chroot 的目錄的相關資訊 */
	struct stat chrdirinfo;      /* 取得原始的 mode 用的 */
	mode_t chrdir_oldmode = -1;  /* 這樣我們才能在結束時 chmod 回去 */

	/* 子程序的 PID，因為不會變，所以複製一份下來就不用去全域變數拿的 */
	pid_t pidcopy;

	/* 和子程序溝通用的 */
	int childpipe[2];
	int childmsg[2]; /* 每個訊息的大小都是 sizeof(int)*2 
						[0]=發生事情的名稱 [1]=errno */
	int readsize = sizeof(int) * 2;

	/* 計時用的 */
	struct timespec execstart, execfinish;

	/* 子程序結束狀態 */
	int childexitstat;
	char childterminated = 0;

	/* 子程序才用的到的變數 */
	int fdinput, fdoutput, fderr, childressize;
	const int fdtablesize = getdtablesize();
	struct rlimit childres;

	/* 變數宣告完了 */

	if(mcopt.flags & SCTMC_VERBOSE){
		puts("列出設定值：");
		sctjudge_list_settings(&mcopt);
		fflush(stdout);
	}
	if(mcopt.flags & SCTMC_DRYRUN){
		abort_makechild(SCTMCRVAL_SUCCESS);
	}
	
	/* 我們要開始做正事了 */
	/* 由此開始，我是唯一會用到 strerror() 的 thread
	 * 所以說應該沒有問題 */
	if(mcopt.chrootdir != NULL && !(mcopt.flags & SCTMC_NOCOPY)){
		/* 必須要複製可執行檔 */
		if(mcopt.flags & SCTMC_VERBOSE){
			puts("開始複製可執行檔......");
		}
		if(copyfilebyname(mcopt.executable, mcopt.chrootdir, &execshortname
					, &execdestname) < 0){
			abort_makechild(SCTMCRVAL_PREPARE);
		}
	}
	/* 再來是 chroot 的問題，避免受測程式亂開目錄之類的，
	 * 所以把權限設成 0555，不過如果沒有變更 uid 和 gid ，
	 * 受測程式還是可以 chmod 回來 */
	if(mcopt.chrootdir != NULL){
		if(mcopt.flags & SCTMC_VERBOSE){
			puts("取得用於 chroot 的目錄相關資訊");
		}
		if(stat(mcopt.chrootdir, &chrdirinfo) < 0){
			fprintf(stderr, "無法取得目錄 %s 的相關資訊：%s\n", 
				mcopt.chrootdir, strerror(errno));
			abort_makechild(SCTMCRVAL_PREPARE);
		}else{
			if((chrdirinfo.st_mode & S_IFMT) == S_IFDIR){
				chrdir_oldmode = chrdirinfo.st_mode &
					(S_ISUID |S_ISGID |S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO);
			}else{
				fprintf(stderr, "%s 並不是目錄\n", mcopt.chrootdir);
				abort_makechild(SCTMCRVAL_PREPARE);
			}
		}
		if(mcopt.flags & SCTMC_VERBOSE){
			puts("嘗試變更此目錄的權限");
		}
		if(chmod(mcopt.chrootdir, 
			S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0){
			fprintf(stderr, "無法變更目錄 %s 的權限\n", mcopt.chrootdir);
			abort_makechild(SCTMCRVAL_PREPARE);
		}
	}

	fflush(stdout);
#ifndef HAVE_CONF_CAP
	enable_setuid();
#endif
	/* 建立用來和 chlid process 溝通的 pipe */
	pipe(childpipe);
	pthread_mutex_lock(&pidmutex);
#ifdef HAVE_WORKING_FORK
	pidchild = fork();
#else
#error "This program requires a working fork function."
#endif
	if(pidchild < 0){
#ifndef HAVE_CONF_CAP
		disable_setuid();
#endif
		pthread_mutex_unlock(&pidmutex);
		fprintf(stderr, "子程序建立失敗：%s\n", strerror(errno));
		abort_makechild(SCTMCRVAL_FORK);
	}
	else if(pidchild > 0){
		/* 我們原本的 thread */
		/* 從這裡開始，我們需要 setuid 來 kill 子程序，
		 * 所以說不要執行 disable_setuid() */
		pidcopy = pidchild;
		pthread_mutex_unlock(&pidmutex);
		close(childpipe[1]);
		/* 從 pipe 接收 child process 狀態，每次固定要讀 sizeof(int)*2 */
		while(read_size(childpipe[0], (void*)childmsg, readsize) == readsize){
			if(childmsg[0] < 0 || childmsg[0] >= SCTCHILD_MSGMAX){
				/* 會發生嗎？除非被亂七八糟 ptrace 吧 
				 * 這時候先砍掉子程序再說 */
				kill(pidcopy, SIGKILL);
				close(childpipe[0]);
				abort_makechild(SCTMCRVAL_INVALID);
			}else{
				if(childmsg[1]){
					fprintf(stderr, "子程序[%d]發生錯誤：%s：%s\n",
							pidcopy, childmsg_text[childmsg[0]], 
							strerror(childmsg[1]));
					kill(pidcopy, SIGKILL);
					close(childpipe[0]);
					abort_makechild(SCTMCRVAL_CHILDFAIL);
				}else{
					if(mcopt.flags & SCTMC_VERBOSE){
						printf("子程序[%d]：%s\n", pidcopy, 
								childmsg_text[childmsg[0]]);
					}
				}
			}
		}
		clock_gettime(CLOCK_REALTIME, &execstart);
		close(childpipe[0]);

		/* 現在我們確定受測程式已經在執行了，所以需要記錄一下時間
		 * 通知其他 thread */
		sem_post(&tlethr);
		sem_post(&dispthr);

		/* 開始要 wait 了，莫名其妙 stop 的程式我最多只會送
		 * 5 次 SIGCONT 給你（避免進入無限迴圈） 
		 * 這種奇怪的程式就等著 SLE 吧 */
		for(i=0; i<=5; i++){
			wait4(pidcopy, &childexitstat, WUNTRACED, &toreturn->judge_rusage);
			if(WIFSTOPPED(childexitstat) && i!=5){
				if(mcopt.flags & SCTMC_VERBOSE){
					printf("\n子程序因訊號 %d 而暫停，自動傳送 SIGCONT "
							"(第 %d 次)\n",
						WSTOPSIG(childexitstat), i+1);
				}
				kill(pidcopy, SIGCONT);
			}else{
				if(WIFSIGNALED(childexitstat)){
					clock_gettime(CLOCK_REALTIME, &execfinish);
					childterminated = 1;
					toreturn->judge_signal = WTERMSIG(childexitstat);
					if(toreturn->judge_signal == SIGXFSZ){
						toreturn->judge_result = SCTRES_OLE;
					}else{
						toreturn->judge_result = SCTRES_RE;
					}
					toreturn->judge_exitcode = WEXITSTATUS(childexitstat);
					break;
				}
				if(WIFEXITED(childexitstat)){
					clock_gettime(CLOCK_REALTIME, &execfinish);
					childterminated = 1;
					toreturn->judge_result = SCTRES_OK;
					toreturn->judge_exitcode = WEXITSTATUS(childexitstat);
					break;
				}
			}
		}
		if(!childterminated){
			kill(pidcopy, SIGKILL);
			wait4(pidcopy, &childexitstat, WUNTRACED, &toreturn->judge_rusage);
			toreturn->judge_result = SCTRES_SLE;
			toreturn->judge_exitcode = WEXITSTATUS(childexitstat);
			clock_gettime(CLOCK_REALTIME, &execfinish);
		}
		pthread_mutex_lock(&judge_tle_mx);
		if(judge_tle && toreturn->judge_result == SCTRES_RE){
			toreturn->judge_result = SCTRES_TLE;
		}
		pthread_mutex_unlock(&judge_tle_mx);
		difftimespec(&execstart, &execfinish, &toreturn->judge_time);
		if(break_flag){
			toreturn->judge_result = SCTRES_AB;
		}
		sctjudge_makechild_cleanup_p1();
		if(mcopt.flags & SCTMC_VERBOSE){
			/* XXX 有更好的方法洗掉文字嗎？ */
			fputs("\r                                                       \r"
					, stdout);
		}
	}else{
		/* 子程序 
		 * 由此開始要很小心，因為我們是在 thread 裡面 fork (?) */
		close(childpipe[0]);
		/* close-on-exec 用來關閉這個暫時的資料傳輸通道 */
		fcntl(childpipe[1], F_SETFD, fcntl(childpipe[1],F_GETFD) | FD_CLOEXEC);

		/* 設為獨立的 process group，不過失敗就算了，這不重要 */
		setpgid(getpid(), getpid());

		/* 重設所有 signal handler
		 * XXX 我猜 signal 最大值是 32，還有其他更好的寫法嗎？ */
		for(i=1; i<=32; i++){
			signal(i, SIG_DFL);
		}

#ifndef HAVE_CONF_CAP
		disable_setuid();
#endif

		fdinput = open(mcopt.inputfile, O_RDONLY);
		if(fdinput < 0){
			SCTCHILD_WRITE_FAILMSG(SCTCHILD_OPEN_INPUT);
			exit(1);
		}else{
			SCTCHILD_WRITE_SUCCMSG(SCTCHILD_OPEN_INPUT);
		}
		if(dup2(fdinput, STDIN_FILENO) < 0){
			SCTCHILD_WRITE_FAILMSG(SCTCHILD_DUP2_INPUT);
			exit(1);
		}else{
			SCTCHILD_WRITE_SUCCMSG(SCTCHILD_DUP2_INPUT);
		}

		/* 注意：開啟輸出檔會直接覆寫現存檔案！ */
		fdoutput = open(mcopt.outputfile, O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR);
		if(fdoutput < 0){
			SCTCHILD_WRITE_FAILMSG(SCTCHILD_OPEN_OUTPUT);
			exit(1);
		}else{
			SCTCHILD_WRITE_SUCCMSG(SCTCHILD_OPEN_OUTPUT);
		}
		if(dup2(fdoutput, STDOUT_FILENO) < 0){
			SCTCHILD_WRITE_FAILMSG(SCTCHILD_DUP2_OUTPUT);
			exit(1);
		}else{
			SCTCHILD_WRITE_SUCCMSG(SCTCHILD_DUP2_OUTPUT);
		}

#ifndef HAVE_CONF_CAP
		enable_setuid();
#endif
		fchown(fdoutput, procrealuid, -1);

		/* 再來就是 stderr 了 */
		if(mcopt.flags & SCTMC_REDIR_STDERR){
			if(dup2(fdoutput, STDERR_FILENO) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_DUP2_STDERR);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_DUP2_STDERR);
			}
		}else{
			fderr = open(WITH_NULL, O_RDONLY);
			if(fderr < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_OPEN_STDERR);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_OPEN_STDERR);
			}
			if(dup2(fderr, STDERR_FILENO) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_DUP2_STDERR);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_DUP2_STDERR);
			}
		}

		/* 很暴力地把所有不該留著的 fd 關掉 */
		for(i=3; i<fdtablesize; i++){
			if(i != childpipe[1]){
				close(i);
			}
		}
		SCTCHILD_WRITE_SUCCMSG(SCTCHILD_CLOSE_OTHERFD);

		/* 有人要求我要 chroot 嗎？ */
		if(mcopt.chrootdir != NULL){
			if(chroot(mcopt.chrootdir) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_CHROOT);
				exit(1);
			}else{
				chdir("/");
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_CHROOT);
			}
		}


#ifdef HAVE_CONF_CAP
		/* 確保修改身份的時候 capabilities 仍然留著
		 * 等一下再自己把 capabilities 丟掉 */
		prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
#endif		

		/* 我要 setgid 嗎？ */
		if(mcopt.flags & SCTMC_SETGID){
			if(setregid(mcopt.gid, mcopt.gid) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_SETGID);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_SETGID);
			}
			if(setgroups(1, &mcopt.gid) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_SETGROUPS);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_SETGROUPS);
			}
		}

		/* 我要 setuid 嗎？ */
		if(mcopt.flags & SCTMC_SETUID){
			if(setreuid(mcopt.uid, mcopt.uid) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_SETUID);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_SETUID);
			}
		}
#ifndef HAVE_CONF_CAP
		else{
			/* 確保 setuid 可執行檔所造成的 effective UID 改變不會傳給子程序 */
			setuid(procrealuid);
		}
#endif

		/* 開始設定資源限制 */
		if(mcopt.memlimit > 0){
			childressize = mcopt.memlimit;
			childres.rlim_cur = childressize;
			childres.rlim_max = childressize;
			if(setrlimit(RLIMIT_AS, &childres) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_SETRLIMIT_VM);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_SETRLIMIT_VM);
			}
		}

		if(mcopt.outlimit > 0){
			childressize = mcopt.outlimit;
			childres.rlim_cur = childressize;
			childres.rlim_max = childressize;
			if(setrlimit(RLIMIT_FSIZE, &childres) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_SETRLIMIT_FSIZE);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_SETRLIMIT_FSIZE);
			}
		}

		/* 從現在開始，不准再開檔案了 */
		childres.rlim_cur = 0;
		childres.rlim_max = 0;
		if(setrlimit(
#ifdef RLIMIT_NOFILE
					RLIMIT_NOFILE
#else
					RLIMIT_OFILE
#endif
					, &childres) < 0){
			SCTCHILD_WRITE_FAILMSG(SCTCHILD_SETRLIMIT_OPENFILE);
			exit(1);
		}else{
			SCTCHILD_WRITE_SUCCMSG(SCTCHILD_SETRLIMIT_OPENFILE);
		}

		/* 從現在開始，不可以再產生新程序了 */
		if(setrlimit(RLIMIT_NPROC, &childres) < 0){
			SCTCHILD_WRITE_FAILMSG(SCTCHILD_SETRLIMIT_PROC);
			exit(1);
		}else{
			SCTCHILD_WRITE_SUCCMSG(SCTCHILD_SETRLIMIT_PROC);
		}
		

		/* 非 Linux 系統就到此結束了，可以 exec 了 */

#ifdef HAVE_CONF_CAP
		/* 正在丟掉 capabilities */
		cap_t nocap = cap_init();
		cap_clear_flag(nocap, CAP_EFFECTIVE);
		cap_clear_flag(nocap, CAP_INHERITABLE);
		cap_clear_flag(nocap, CAP_PERMITTED);
		if(cap_set_proc(nocap) < 0){
			SCTCHILD_WRITE_FAILMSG(SCTCHILD_CAPSET);
			exit(1);
		}else{
			SCTCHILD_WRITE_SUCCMSG(SCTCHILD_CAPSET);
		}
		/* 需要 drop bound 嗎？再考慮看看 */
#endif

		if(mcopt.chrootdir == NULL){
			execl(mcopt.executable, mcopt.executable, NULL);
		}else{
			if(mcopt.flags & SCTMC_NOCOPY){
				execl(mcopt.executable, mcopt.executable, NULL);
			}else{
				execl(execshortname, execshortname, NULL);
			}
		}

		SCTCHILD_WRITE_FAILMSG(SCTCHILD_EXEC);
		exit(2);
	}
	/* Judge 完成，準備離開 */
	fflush(stdout);
	sctjudge_makechild_cleanup_p2(&mcopt, chrdir_oldmode, execdestname);
	fflush(stdout);
	(toreturn->mc_exitcode) = SCTMCRVAL_SUCCESS;
	return (void*)toreturn;
}
#endif
