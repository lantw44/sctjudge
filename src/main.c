#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "config2.h"
#include "version.h"
#include "common.h"
#include "sctcore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <pthread.h>
#include <semaphore.h>

#include <l4darr.h>
#include <l4arg.h>

#define SCTMAIN_ERRNOARG "%s: 選項「%s」之後需要一個參數\n"
#define SCTMAIN_ERRNOQARG "%s: 在「%s」之後缺少一個值\n"
#define SCTMAIN_ERREXTRAQARG "%s: 在「%s」之後並不需要參數\n"

#define SCTMAIN_CHECKARGEXIST \
	do{ \
		i++; \
		if(i >= argc){ \
			fprintf(stderr, SCTMAIN_ERRNOARG, argv[0], argv[i-1]); \
			exit(SCTEXIT_SYNTAX); \
		} \
	}while(0) 

#define SCTMAIN_CHECKQARGHAVEVALUE \
	do{ \
		if(!l4qarg_hasvalue(qarglist[j])){ \
			fprintf(stderr, SCTMAIN_ERRNOQARG, \
					argv[0], qarglist[j].arg_name); \
			exit(SCTEXIT_SYNTAX); \
		} \
	}while(0)

#define SCTMAIN_CHECKQARGNOVALUE \
	do{ \
		if(l4qarg_hasvalue(qarglist[j])){ \
			fprintf(stderr, SCTMAIN_ERREXTRAQARG, \
					argv[0], qarglist[j].arg_name); \
			exit(SCTEXIT_SYNTAX); \
		} \
	}while(0)

static const char* sctres_text[2][SCTRES_MAX] = {
	{"OK", "RE", "TLE", "OLE", "SLE", "AB", "UD"},
	{
		"正常結束",
		"執行過程中發生錯誤",
		"超過時間限制",
		"輸出超過限制",
		"暫停次數太多",
		"使用者中斷操作",
		"未定義的結果"
	}
};

int main(int argc, char* argv[]){
	/* 照理說是 zh_TW.utf8，但考慮作業系統差異還是從環境變數抓
	 * （反正出錯是使用者的事） */
	setlocale(LC_ALL, ""); 

	int i, j;
	struct makechildopt mcopt; /* 送給 sctjudge_makechild 當參數用 */
	char verbose=0; /* 0 是安靜模式（機器模式）、1 是 verbose、2 是 debug */
	char dryrun=0, force=0;
	L4QARG* qarglist;
#ifdef HAVE_CONF_UGIDNAME
	struct passwd* pwinfo;
	struct group* grinfo;
#endif

	/* 預設值當然都是 0 */
	memset(&mcopt, 0 ,sizeof(mcopt));
	/* 據說有些 NULL 不是 0，所以還是設定一下比較保險 */
	mcopt.executable = NULL;
	mcopt.chrootdir = NULL;
	mcopt.inputfile = NULL;
	mcopt.outputfile = NULL;

	/* 解析命令列的時間到了 */
	for(i=1; i<argc; i++){
		if(argv[i][0] == '-'){
			if(!strcmp(&argv[i][1], "help") || !strcmp(&argv[i][1], "-help")){
				printf(SCTJUDGE_TITLEBAR"\n\n"
					"用法：%s [選項]\n"
					"選項：\n\n"
					"  -v/-verbose\n"
					"      顯示更多訊息（第二個 -v 可以顯示所有重要指令執行過"
					"程）\n\n"
					"  -V/version\n"
					"      顯示版本資訊並查看可使用的額外功能列表\n\n"
					"  -n/-dryrun\n"
					"      只列出設定值而不要執行（包含 -v）\n\n"
					"  -f/-force\n"
					"      忽略警告訊息並強制執行\n\n"
					"  -t/-time <時間>\n"
					"      受測程式時間限制為<時間>毫秒\n\n"
					"  -e/-exec <檔案>\n"
					"      指定受測程式可執行檔名稱（務必靜態連結！）\n\n"
					"  -i/-input <檔案>\n"
					"      指定要導向至受測程式標準輸入的檔案，若未指定則為 "
					NULL_DEVICE"\n\n"
					"  -o/-output <檔案>\n"
					"      指定要導向至受測程式標準輸出的檔案，若未指定則為 "
					NULL_DEVICE"\n\n"
					"  -r/-root <目錄>\n"
					"      受測程式將以<目錄>為根目錄執行，若無此選項則關"
					"閉 chroot 功能\n\n"
					"  -m/-misc 選項1[=值],選項2[=值],選項3[=值]...\n"
					"      設定額外參數：\n\n"
					"      mem=<大小>\n"
					"          設定受測程式記憶體上限為<大小>位元組\n\n"
					"      outlimit=<大小>\n"
					"          受測程式最多只能輸出<大小>位元組，若未指定則不"
					"限制\n"
					"          （無限制時請確定有足夠的磁碟空間！）\n\n"
					"      stderr\n"
					"          將受測程式的標準錯誤也導向至輸出檔。若未指定，"
					"則只有標準輸\n"
					"          出會寫入輸出檔案，標準錯誤則被導向至 "
					NULL_DEVICE"\n\n"
					"      nocopy\n"
					"          如果啟用了 chroot 功能，預設情況下本程式會自動"
					"將檔案複製\n"
					"          到新的根目錄中，並在結束時自動刪除檔案。\n"
					"          使用此選項則取消自動複製的功能，但請注意：此時"
					" file 指定的\n"
					"          檔案名稱是相對於新的根目錄而不是現在的根目錄！"
					"\n\n"
					"      uid=<UID>\n"
					"          受測程式將以 UID 為 <UID> 的使用者身份執行\n\n"
					"      gid=<GID>\n"
					"          受測程式將以 GID 為 <GID> 的群組身份執行\n"
					"          此選項會同時設定 real/effective/supplementary "
					"GID(s)\n"
					"          原有的 supplementry GIDs 會被清空，只剩下這裡指"
					"定的數值\n\n"
					, argv[0]);
				return SCTEXIT_SUCCESS;
			}else if(!strcmp(&argv[i][1], "V") ||
					!strcmp(&argv[i][1], "version") ||
					!strcmp(&argv[i][1], "-version")){
				puts(SCTJUDGE_TITLEBAR);
				putchar('\n');
				printf("目前可用(+)和不可用(-)的功能列表：\n"
#ifdef HAVE_CONF_PROCMON
						"  + "
#else
						"  - "
#endif
						"使用 Linux 的 proc 檔案系統來增強程序監視器的功能 (--"
#ifdef HAVE_CONF_PROCMON
						"enable"
#else
						"disable"
#endif
						"-procmon)\n"
#ifdef HAVE_CONF_CAP
						"  + "
#else
						"  - "
#endif
						"支援 Linux capabilities (--"
#ifdef HAVE_CONF_CAP
						"enable"
#else
						"disable"
#endif
						"-cap)\n"
#ifdef HAVE_CONF_UGIDNAME
						"  + "
#else
						"  - "
#endif
						"自動轉換使用者或群組名稱成 UID 或 GID "
						"(自動偵測)\n"
						);
				putchar('\n');
				printf("編譯時指定的參數列表：\n"
#ifdef NULL_DEVICE
						"  * 用於捨棄資料的裝置檔案："
						NULL_DEVICE
						" (--with-null)\n"
#endif
#ifdef PROC_PATH
						"  * Linux 的 proc 檔案系統路徑："
						PROC_PATH
						" (--with-proc)\n"
#endif
						);
				exit(SCTEXIT_SUCCESS);
			}else if(!strcmp(&argv[i][1], "v") || 
					!strcmp(&argv[i][1], "verbose")){
				verbose++;
				mcopt.flags |= SCTMC_VERBOSE;
				if(verbose >= 2){
					mcopt.flags |= SCTMC_DEBUG;
				}
			}else if(!strcmp(&argv[i][1], "n") || 
					!strcmp(&argv[i][1], "dryrun")){
				dryrun = 1, verbose = 1;
				mcopt.flags |= (SCTMC_DRYRUN | SCTMC_VERBOSE);
			}else if(!strcmp(&argv[i][1], "f") ||
					!strcmp(&argv[i][1], "force")){
				force = 1;
				mcopt.flags |= (SCTMC_FORCE);
			}else if(!strcmp(&argv[i][1], "t") ||
					!strcmp(&argv[i][1], "time")){
				SCTMAIN_CHECKARGEXIST;
				mcopt.exectime = atoi(argv[i]);
				if(mcopt.exectime <= 0){
					fprintf(stderr, "%s: 「%s」不是正確的時間設定值\n", 
							argv[0], argv[i]);
					exit(SCTEXIT_SYNTAX);
				}
			}else if(!strcmp(&argv[i][1], "e") ||
					!strcmp(&argv[i][1], "exec")){
				SCTMAIN_CHECKARGEXIST;
				mcopt.executable = argv[i];
			}else if(!strcmp(&argv[i][1], "i") ||
					!strcmp(&argv[i][1], "in") ||
					!strcmp(&argv[i][1], "input")){
				SCTMAIN_CHECKARGEXIST;
				mcopt.inputfile = argv[i];
			}else if(!strcmp(&argv[i][1], "o") ||
					!strcmp(&argv[i][1], "out") ||
					!strcmp(&argv[i][1], "output")){
				SCTMAIN_CHECKARGEXIST;
				mcopt.outputfile = argv[i];
			}else if(!strcmp(&argv[i][1], "r") ||
					!strcmp(&argv[i][1], "root") ||
					!strcmp(&argv[i][1], "chroot")){
				SCTMAIN_CHECKARGEXIST;
				mcopt.chrootdir = argv[i];
			}else if(!strcmp(&argv[i][1], "m") ||
					!strcmp(&argv[i][1], "misc") ||
					!strcmp(&argv[i][1], "opt") ||
					!strcmp(&argv[i][1], "option")){
				SCTMAIN_CHECKARGEXIST;
				qarglist = l4qarg_parse(argv[i]);
				if(qarglist == NULL){
					fprintf(stderr, "%s: 記憶體配置發生錯誤\n", argv[0]);
					exit(SCTEXIT_MALLOC);
				}
				for(j=0; !l4qarg_end(qarglist[j]); j++){
					if(!strcmp(qarglist[j].arg_name, "mem") ||
						!strcmp(qarglist[j].arg_name, "memory")){
						SCTMAIN_CHECKQARGHAVEVALUE;
						mcopt.memlimit = atoi(qarglist[j].arg_value);
						if(mcopt.memlimit <= 0){
							fprintf(stderr, "%s: 「%s」不是正確的記憶體限制"
									"設定值\n", argv[0], 
									qarglist[j].arg_value);
							exit(SCTEXIT_SYNTAX);
						}
					}else if(!strcmp(qarglist[j].arg_name, "outlimit") ||
							!strcmp(qarglist[j].arg_name, "outlim")){
						SCTMAIN_CHECKQARGHAVEVALUE;
						mcopt.outlimit = atoi(qarglist[j].arg_value);
						if(mcopt.outlimit <= 0){
							fprintf(stderr, "%s: 「%s」不是正確的輸出限制"
									"設定\n", argv[0], qarglist[j].arg_value);
							exit(SCTEXIT_SYNTAX);
						}
					}else if(!strcmp(qarglist[j].arg_name, "stderr")){
						SCTMAIN_CHECKQARGNOVALUE;
						mcopt.flags |= SCTMC_REDIR_STDERR;
					}else if(!strcmp(qarglist[j].arg_name, "nocopy")){
						SCTMAIN_CHECKQARGNOVALUE;
						mcopt.flags |= SCTMC_NOCOPY;
					}else if(!strcmp(qarglist[j].arg_name, "uid") ||
							!strcmp(qarglist[j].arg_name, "user")){
						SCTMAIN_CHECKQARGHAVEVALUE;
						mcopt.flags |= SCTMC_SETUID;
						if(sscanf(qarglist[j].arg_value, "%u", &mcopt.uid) 
								<= 0){
#ifdef HAVE_CONF_UGIDNAME
							pwinfo = getpwnam(qarglist[j].arg_value);
							if(pwinfo == NULL){
								fprintf(stderr, "%s: 「%s」不是正確的 UID 或"
										"使用者名稱\n", 
										argv[0], qarglist[j].arg_value);
								exit(SCTEXIT_SYNTAX);
							}
							mcopt.uid = pwinfo->pw_uid;
#else
							fprintf(stderr, "%s: 「%s」並不是整數\n",
									argv[0], qarglist[j].arg_value);
							exit(SCTEXIT_SYNTAX);
#endif
						}
					}else if(!strcmp(qarglist[j].arg_name, "gid") ||
							!strcmp(qarglist[j].arg_name, "group")){
						SCTMAIN_CHECKQARGHAVEVALUE;
						mcopt.flags |= SCTMC_SETGID;
						if(sscanf(qarglist[j].arg_value, "%u", &mcopt.gid) 
								<= 0){
#ifdef HAVE_CONF_UGIDNAME
							grinfo = getgrnam(qarglist[j].arg_value);
							if(grinfo == NULL){
								fprintf(stderr, "%s: 「%s」不是正確的 GID 或"
										"群組名稱\n", 
										argv[0], qarglist[j].arg_value);
								exit(SCTEXIT_SYNTAX);
							}
							mcopt.gid = grinfo->gr_gid;
#else
							fprintf(stderr, "%s: 「%s」並不是整數\n",
									argv[0], qarglist[j].arg_value);
							exit(SCTEXIT_SYNTAX);
#endif
						}
					}else{
						fprintf(stderr, "%s: 「%s」是不明的選項\n", argv[0],
								qarglist[j].arg_name);
						exit(SCTEXIT_SYNTAX);
					}
				}
				l4qarg_free(qarglist);
			}else{
				fprintf(stderr, "%s: 不明的選項「%s」\n", argv[0], argv[i]);
				return SCTEXIT_SYNTAX;
			}
		}else{
			fprintf(stderr, "%s: 參數 %d「%s」是多餘的\n"
				"請嘗試執行 %s -help 來取得說明\n"
				, argv[0], i, argv[i], argv[0]);
			return SCTEXIT_SYNTAX;
		}
	}
	/* 至此可以進入主程式了 */
	if(verbose){
		puts(SCTJUDGE_TITLEBAR"\n");
	}

#ifndef HAVE_CONF_CAP
	/* 即使有 setuid root，還是不能讓每個使用者拿到 root 權限
	 * 所以說只有在 fork 的時候才要把這個權限開起來，其他就關掉吧 */
	save_uids();
	disable_setuid();
#endif

	/* 檢查與修正設定值 */
	if(verbose){
		puts("正在檢查設定值是否正確......");
	}
	if(mcopt.executable == NULL){
		fputs("受測程式可執行檔名稱為必要參數，不可為空白\n", stderr);
		exit(SCTEXIT_TOOFEW);
	}
	if(mcopt.inputfile == NULL){
		mcopt.inputfile = NULL_DEVICE;
	}
	if(mcopt.outputfile == NULL){
		fputs("輸出檔案名稱必要參數，不可為空白\n", stderr);
		exit(SCTEXIT_TOOFEW);
	}
	if(mcopt.exectime <= 0){
		fputs("執行時間限制為必要參數，不可為空白！\n", stderr);
		exit(SCTEXIT_TOOFEW);
	}
	if((mcopt.flags & SCTMC_SETUID)){
		if(mcopt.uid == 0){
			if(!force){
				fputs("將 UID 設為 0 並不安全！（加上 -f 來強制執行）\n", 
						stderr);
				exit(SCTEXIT_BADID);
			}else{
				if(getuid() != 0){
					fputs("只有 root 可以將 UID 設定 0\n", stderr);
					exit(SCTEXIT_BADID);
				}
			}
		}
	}else{
		if(getuid() == 0 && !force){
			fputs("不允許以 root 身份執行本程式（加上 -f 來強制執行）\n", 
					stderr);
			exit(SCTEXIT_BADID);
		}
	}

	/* 現在開始建立 thread 了！*/
	pthread_t tmain; /* 其他 thread 的 ID 放在全域變數 */
	struct makechildrval* mtreturn; /* 接收 sctjudge_makechild 的回傳值 */

	/* 傳給另外兩個 thread 的參數 */
	int exectimelimit = mcopt.exectime;
	/* XXX 抱歉 hard coding，我以後會改進 */
	long displayinterval = SCT_DISPTIME_INTERVAL; 

	pthread_attr_t detstate, joistate; 
	/* 除了主要的 sctjudge_makechild 以外，
	 * 我想其他的 thread 沒有 join 的必要 */
	pthread_attr_init(&detstate);
	pthread_attr_setdetachstate(&detstate, PTHREAD_CREATE_DETACHED);
	pthread_attr_init(&joistate);
	pthread_attr_setdetachstate(&joistate, PTHREAD_CREATE_JOINABLE);
	pthread_mutex_init(&pidmutex, NULL);
	pthread_mutex_init(&tkill_mx, NULL);
	pthread_mutex_init(&tdisplay_mx, NULL);
	pthread_mutex_init(&judge_tle_mx, NULL);
	sem_init(&mcthr, 0, 0);
	sem_init(&addthr, 0, 0);
	pthread_create(&tmain, &joistate, &sctjudge_makechild, (void*)&mcopt);
	if(!dryrun){
		pthread_create(&tkill, &detstate, &sctjudge_checktle, 
				(void*)&exectimelimit);
	}
	if(!dryrun && verbose){
		pthread_create(&tdisplay, &detstate, &sctjudge_dispaytime,
				(void*)&displayinterval);
	}
	pthread_attr_destroy(&detstate);
	pthread_attr_destroy(&joistate);
	sem_post(&mcthr); 

	/* 這個 semaphore 到這裡就沒用了， 
	 * sctjudge_makechild 一旦開始動就會把它 destroy 掉 */
	pthread_join(tmain, (void**)&mtreturn);
	/* XXX 底下這訊息很有可能讓使用者覺得很奇怪 */
	pthread_mutex_destroy(&pidmutex);
	pthread_mutex_destroy(&tkill_mx);
	pthread_mutex_destroy(&tdisplay_mx);

	if(dryrun){
		return SCTEXIT_SUCCESS;
	}

	/* 這裡就要顯示結果了 */
	if(mtreturn == NULL || mtreturn->mc_exitcode != 0){
		fprintf(stderr, "%s: 因錯誤發生而結束程式\n", argv[0]);
		free((void*)mtreturn);
		sem_destroy(&mcthr);
		exit(SCTEXIT_THREAD);
	}else{
		if(verbose){
			putchar('\r');
			puts("=======================================================");
			fputs("評測結果：", stdout);
			if(mtreturn->judge_result < 0 || 
					mtreturn->judge_result >= SCTRES_UD){
				mtreturn->judge_result = SCTRES_UD;
			}
			printf("%s (%s)\n", 
					sctres_text[0][mtreturn->judge_result],
					sctres_text[1][mtreturn->judge_result]);
			printf("執行時間：%ld.%03ld 秒\n",
					mtreturn->judge_time.tv_sec,
					mtreturn->judge_time.tv_nsec / 1000000);
			printf("程式離開狀態：%d\n", mtreturn->judge_exitcode);
			if(mtreturn->judge_result == SCTRES_RE){
				printf("程式因訊號 %d 而終止\n", mtreturn->judge_signal);
			}
			puts("-------------------------------------------------------");
			printf("User CPU time: %ld.%03ld\n"
					"System CPU time: %ld.%03ld\n",
					mtreturn->judge_rusage.ru_utime.tv_sec,
					mtreturn->judge_rusage.ru_utime.tv_usec / 1000,
					mtreturn->judge_rusage.ru_stime.tv_sec,
					mtreturn->judge_rusage.ru_stime.tv_usec / 1000);
			puts("=======================================================");
		}else{
			/* 輸出格式：
			 * 第一行是結果代碼、時間、離開狀態 
			 * 第二行是收到的訊號 (只在 RE 時會有) */
			if(mtreturn->judge_result < 0 || 
					mtreturn->judge_result >= SCTRES_UD){
				mtreturn->judge_result = SCTRES_UD;
			}
			fputs(sctres_text[0][mtreturn->judge_result], stdout);
			putchar(' ');
			printf("%ld%03ld %d\n", mtreturn->judge_time.tv_sec,
					mtreturn->judge_time.tv_nsec / 1000000,
					mtreturn->judge_exitcode);
			if(mtreturn->judge_result == SCTRES_RE){
				printf("%d\n", mtreturn->judge_signal);
			}
		}
		free((void*)mtreturn);
	}
	return SCTEXIT_SUCCESS;
}
