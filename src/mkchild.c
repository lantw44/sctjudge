#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "config2.h"
#include "common.h"
#include "sctcore.h"

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

#define abort_makechild(val) \
	do { \
	sctjudge_makechild_cleanup(&mcopt, chrdir_oldmode, execdestname); \
	toreturn->mc_exitcode = (val); \
	pthread_exit((void*)toreturn); \
	}while(0)

#define SCTCHILD_OPEN_INPUT			 0
#define SCTCHILD_DUP2_INPUT			 1
#define SCTCHILD_OPEN_OUTPUT		 2
#define SCTCHILD_DUP2_OUTPUT		 3
#define SCTCHILD_OPEN_STDERR		 4
#define SCTCHILD_DUP2_STDERR		 5
#define SCTCHILD_CLOSE_OTHERFD		 6
#define SCTCHILD_SETRLIMIT_VM		 7
#define SCTCHILD_SETRLIMIT_FSIZE	 8
#define SCTCHILD_SETRLIMIT_OPENFILE	 9
#define SCTCHILD_SETRLIMIT_PROC		10
#define SCTCHILD_CHROOT				11
#define SCTCHILD_SETUID				12
#define SCTCHILD_SETGID				13
#define SCTCHILD_SETGROUPS			14
#define SCTCHILD_CAPSET				15
#define SCTCHILD_EXEC				16
#define SCTCHILD_MSGMAX				17 /* 訊息代碼的最大值 */
#define SCTCHILD_WRITE_SUCCMSG(reason) \
	do{ \
		childmsg[0] = (reason); \
		childmsg[1] = 0; \
		write(childpipe[1], (void*)childmsg, sizeof(int)*2);  \
	}while(0)
#define SCTCHILD_WRITE_FAILMSG(reason) \
	do{ \
		childmsg[0] = (reason); \
		childmsg[1] = errno; \
		write(childpipe[1], (void*)childmsg, sizeof(int)*2);  \
	}while(0)

pid_t pidchild;
pthread_mutex_t pidmutex, tkill_mx, tdisplay_mx, judge_tle_mx;
pthread_t tkill, tdisplay;
char tkill_yes = 0, tdisplay_yes = 0, judge_tle = 0;
sem_t mcthr, addthr;

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
		pthread_cancel(tkill);
	}else{
		pthread_mutex_unlock(&tkill_mx);
	}

	pthread_mutex_lock(&tdisplay_mx);
	if(tdisplay_yes){
		tdisplay_yes = 0;
		pthread_mutex_unlock(&tdisplay_mx);
		pthread_cancel(tdisplay);
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
	sem_destroy(&mcthr);

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
		 * 通知其他 thread，然後就可以把 semaphore 丟掉了 */
		sem_post(&addthr);
		sem_post(&addthr);
		sem_destroy(&addthr);

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
		fchown(fdoutput, getuid(), -1);

		/* 再來就是 stderr 了 */
		if(mcopt.flags & SCTMC_REDIR_STDERR){
			if(dup2(fdoutput, STDERR_FILENO) < 0){
				SCTCHILD_WRITE_FAILMSG(SCTCHILD_DUP2_STDERR);
				exit(1);
			}else{
				SCTCHILD_WRITE_SUCCMSG(SCTCHILD_DUP2_STDERR);
			}
		}else{
			fderr = open(NULL_DEVICE, O_RDONLY);
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
			setuid(getuid());
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
