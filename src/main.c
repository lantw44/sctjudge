#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "config2.h"
#include "version.h"
#include "common.h"
#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <pthread.h>
#include <semaphore.h>

#define SCTMAIN_ERRNOARG "%s: 選項「%s」之後需要一個參數"
#define SCTMAIN_ERRBADARG "%s: 在「%s」之後有個不存在或用"\
	"法不正確的參數「%s」\n"

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

int getargvint(int* ip, int argc, const char* prearg, const char* arg, 
		const char* argzero, const char* invalidmsg){
	int toreturn;
	(*ip)++;
	if((*ip) < argc){
		toreturn = atoi(arg);
		if(toreturn <= 0){
			fprintf(stderr, invalidmsg, argzero, arg);
			exit(SCTJUDGE_EXIT_SYNTAX);
		}
	}else{
		fprintf(stderr, SCTMAIN_ERRNOARG, argzero, prearg);
		exit(SCTJUDGE_EXIT_SYNTAX);
	}
	return toreturn;
}

char* getargvstr(int* ip, int argc, const char* prearg, const char* arg, 
		const char* argzero){
	char* toreturn;
	(*ip)++;
	if((*ip) < argc){
		toreturn = (char*)arg;
	}else{
		fprintf(stderr, SCTMAIN_ERRNOARG, argzero, prearg);
		exit(SCTJUDGE_EXIT_SYNTAX);
	}
	return toreturn;
}

int getargvmulstr(char* arg, char** key, char** value, int* keylen){
	/* 警告；這個函式 thread unsafe */
	static char* prearg = NULL;
	char *tmpstr, *seppos;
	if(prearg != arg){
		tmpstr = strtok(arg, ",");
	}else{
		tmpstr = strtok(NULL, ",");
	}
	prearg = arg;
	if(tmpstr == NULL){
		return -1;
	}else{
		seppos = strchr(tmpstr, '=');
		(*key) = tmpstr;
		if(seppos == NULL){
			(*value) = NULL;
		}else{
			(*keylen) = seppos - tmpstr;
			(*value) = seppos + 1;
		}
		return 0;
	}
	return 0;
}

int main(int argc, char* argv[]){
	/* 照理說是 zh_TW.utf8，但考慮作業系統差異還是從環境變數抓
	 * （反正出錯是使用者的事） */
	setlocale(LC_ALL, ""); 

	int i;
	struct makechildopt mcopt; /* 送給 sctjudge_makechild 當參數用 */
	char verbose=0, dryrun=0;
	char *mulstr_key, *mulstr_value;
	int mulstr_keylen;

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
					"      顯示詳細執行過程\n\n"
					"  -version\n"
					"      顯示版本資訊\n\n"
					"  -n/-dryrun\n"
					"      只列出設定值而不要執行（包含 -v）\n\n"
					"  -t/-time <時間>\n"
					"      受測程式時間限制為<時間>毫秒\n\n"
					"  -m/-memory <大小>\n"
					"      受測程式記憶體限制<大小>MiB\n\n"
					"  -i/-input <檔案>\n"
					"      指定要導向至受測程式標準輸入的檔案，若未指定則為 "
					NULL_DEVICE"\n\n"
					"  -o/-output file=<檔案>[,stderr][,limit=<大小>]\n"
					"      設定受測程式輸出選項：\n\n"
					"      file=<檔案>\n"
					"          指定輸出檔案為<檔案>\n\n"
					"      stderr\n"
					"          將受測程式的標準錯誤也導向至輸出檔。若未指定，"
					"則只有標準輸\n"
					"          出會寫入輸出檔案，標準錯誤則被導向至 "
					NULL_DEVICE"\n\n"
					"      limit=<大小>\n"
					"          受測程式最多只能輸出<大小>MiB，若未指定則不限制"
					"\n"
					"          （無限制時請確定你有足夠的磁碟空間！）\n\n"
					"  -e/-exec file=<檔案>[,nocopy]\n"
					"      設定受測程式可執行檔位置：\n\n"
					"      file=<檔案>\n"
					"          指定受測程式檔案名稱\n\n"
					"      nocopy\n"
					"          如果你啟用了 chroot 功能，預設情況下本程式會自"
					"動將檔案複製\n"
					"          到新的根目錄中，並在結束時自動刪除檔案。\n"
					"          使用此選項則取消自動複製的功能，但請注意：此時"
					" file 指定的\n"
					"          檔案名稱是相對於新的根目錄而不是現在的根目錄！"
					"\n\n"
					"  -chroot <目錄>\n"
					"          受測程式將以<目錄>為根目錄執行，若無此選項則關"
					"閉 chroot 功能\n\n"
					"  -u/-uid <UID>\n"
					"          受測程式將以 UID 為 <UID> 的使用者身份執行\n"
					"  -g/-gid <GID>\n"
					"          受測程式將以 GID 為 <GID> 的群組身份執行\n"
					"          此選項會同時設定 real/effective/supplementary "
					"GID(s)\n"
					"          原有的 supplementry GIDs 會被清空，只剩下這裡指"
					"定的數值\n\n"
					, argv[0]);
				return SCTJUDGE_EXIT_SUCCESS;
			}else if(!strcmp(&argv[i][1], "version") ||
					!strcmp(&argv[i][1], "-version")){
				puts(SCTJUDGE_TITLEBAR);
				exit(SCTJUDGE_EXIT_SUCCESS);
			}else if(!strcmp(&argv[i][1], "v") || 
					!strcmp(&argv[i][1], "verbose")){
				verbose = 1;
				mcopt.flags |= SCTMC_VERBOSE;
			}else if(!strcmp(&argv[i][1], "n") || 
					!strcmp(&argv[i][1], "dryrun")){
				dryrun = 1, verbose = 1;
				mcopt.flags |= (SCTMC_DRYRUN | SCTMC_VERBOSE);
			}else if(!strcmp(&argv[i][1], "t") ||
					!strcmp(&argv[i][1], "time")){
				mcopt.exectime = getargvint(&i, argc, argv[i], argv[i+1], 
						argv[0], "%s: 「%s」不是正確的時間設定值\n");
			}else if(!strcmp(&argv[i][1], "m") ||
					!strcmp(&argv[i][1], "memory")){
				mcopt.memlimit = getargvint(&i, argc, argv[i], argv[i+1],
						argv[0], "%s: 「%s」不是正確的記憶體大小設定\n");
			}else if(!strcmp(&argv[i][1], "i") ||
					!strcmp(&argv[i][1], "input")){
				mcopt.inputfile = getargvstr(&i, argc, argv[i], argv[i+1],
					   	argv[0]);
			}else if(!strcmp(&argv[i][1], "o") ||
					!strcmp(&argv[i][1], "output")){
				getargvstr(&i, argc, argv[i], argv[i+1], argv[0]);
				while(getargvmulstr(argv[i], &mulstr_key, &mulstr_value,
							&mulstr_keylen) >= 0){
					if(mulstr_value == NULL){
						if(!strcmp(mulstr_key, "stderr")){
							mcopt.flags |= SCTMC_REDIR_STDERR;
						}else{
							fprintf(stderr, SCTMAIN_ERRBADARG
									, argv[0], argv[i-1], mulstr_key);
							exit(SCTJUDGE_EXIT_SYNTAX);
						}
					}else{
						if(!strncmp(mulstr_key, "limit", mulstr_keylen)){
							mcopt.outlimit = atoi(mulstr_value);
							if(mcopt.outlimit <= 0){
								fprintf(stderr, "%s: 「%s」不是正確的輸出限制"
										"值\n", argv[0], mulstr_value);
								exit(SCTJUDGE_EXIT_SYNTAX);
							}
						}else if(!strncmp(mulstr_key, "file", mulstr_keylen)){
							mcopt.outputfile = mulstr_value;
						}else{
							fprintf(stderr, SCTMAIN_ERRBADARG
									, argv[0], argv[i-1], mulstr_key);
							exit(SCTJUDGE_EXIT_SYNTAX);
						}
					}
				}
			}else if(!strcmp(&argv[i][1], "e") ||
					!strcmp(&argv[i][1], "exec")){
				getargvstr(&i, argc, argv[i], argv[i+1], argv[0]);
				while(getargvmulstr(argv[i], &mulstr_key, &mulstr_value, 
							&mulstr_keylen) >= 0){
					if(mulstr_value == NULL){
						if(!strcmp(mulstr_key, "nocopy")){
							mcopt.flags |= SCTMC_NOCOPY;
						}else{
							fprintf(stderr, SCTMAIN_ERRBADARG, 
									argv[0], argv[i-1], mulstr_key);
							exit(SCTJUDGE_EXIT_SYNTAX);
						}
					}else{
						if(!strncmp(mulstr_key, "file", mulstr_keylen)){
							mcopt.executable = mulstr_value;
						}else{
							fprintf(stderr, SCTMAIN_ERRBADARG
									, argv[0], argv[i-1], mulstr_key);
							exit(SCTJUDGE_EXIT_SYNTAX);
						}
					}
				}
			}else if(!strcmp(&argv[i][1], "chroot")){
				mcopt.chrootdir = getargvstr(&i, argc, argv[i], argv[i+1],
						argv[0]);
			}else if(!strcmp(&argv[i][1], "u") || !strcmp(&argv[i][1], "uid")){
				mcopt.flags |= SCTMC_SETUID;
				mcopt.uid = getargvint(&i, argc, argv[i], argv[i+1],
						argv[0], "%s: 指定 UID 為「%s」不安全或不合理\n");
			}else if(!strcmp(&argv[i][1], "g") || !strcmp(&argv[i][1], "gid")){
				mcopt.flags |= SCTMC_SETGID;
				mcopt.gid = getargvint(&i, argc, argv[i], argv[i+1],
						argv[0], "%s: 指定 GID 為「%s」不安全或不合理\n");
			}else{
				fprintf(stderr, "%s: 不明的選項「%s」\n", argv[0], argv[i]);
				return SCTJUDGE_EXIT_SYNTAX;
			}
		}else{
			fprintf(stderr, "%s: 參數 %d「%s」是多餘的\n"
				"請嘗試執行 %s -help 來取得說明\n"
				, argv[0], i, argv[i], argv[0]);
			return SCTJUDGE_EXIT_SYNTAX;
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
		exit(SCTJUDGE_EXIT_TOOFEW);
	}
	if(mcopt.inputfile == NULL){
		mcopt.inputfile = NULL_DEVICE;
	}
	if(mcopt.outputfile == NULL){
		fputs("輸出檔案名稱必要參數，不可為空白\n", stderr);
		exit(SCTJUDGE_EXIT_TOOFEW);
	}
	if(mcopt.exectime <= 0){
		fputs("執行時間限制為必要參數，不可為空白！\n", stderr);
		exit(SCTJUDGE_EXIT_TOOFEW);
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
		return SCTJUDGE_EXIT_SUCCESS;
	}

	/* 這裡就要顯示結果了 */
	if(mtreturn == NULL || mtreturn->mc_exitcode != 0){
		fprintf(stderr, "%s: 因錯誤發生而結束程式\n", argv[0]);
		free((void*)mtreturn);
		sem_destroy(&mcthr);
		exit(SCTJUDGE_EXIT_THREAD);
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
	return SCTJUDGE_EXIT_SUCCESS;
}
