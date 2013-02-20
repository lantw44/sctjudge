#ifdef HAVE_CONFIG_H
# include "SctConfig.h"
#endif

#include "SctConst.h"
#include "SctVersion.h"
#include "SctCommon.h"
#include "ProcCommon.h"
#include "JudgeCommon.h"
#include "CliCommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <pthread.h>
#include <semaphore.h>

#ifdef HAVE_CONF_UGIDNAME
#include <grp.h>
#include <pwd.h>
#endif

#include <l4darr.h>
#include <l4arg.h>

#define SCTJUDGE_CLIARG_CHECK_MISSING \
	if(++i >= argc){ \
		fprintf(stderr, "%s: 選項「%s」之後需要一個參數\n", \
				argv[0], argv[i-1]); \
		exit(SCTJUDGE_EXIT_SYNTAX); \
	}

#define SCTJUDGE_CLIARG_CHECK_QARG_MISSING \
	if(!l4qarg_hasvalue(qarglist[j])){ \
		fprintf(stderr, "%s: 在「%s」之後缺少一個值\n", \
				argv[0], qarglist[j].arg_name); \
		exit(SCTJUDGE_EXIT_SYNTAX); \
	}

#define SCTJUDGE_CLIARG_CHECK_QARG_EXTRA \
	if(l4qarg_hasvalue(qarglist[j])){ \
		fprintf(stderr, "%s: 在「%s」之後並不需要參數\n", \
				argv[0], qarglist[j].arg_name); \
		exit(SCTJUDGE_EXIT_SYNTAX); \
	} \

typedef struct misc_option_flag_and_number {
	char*     id;
	unsigned* num;
	unsigned  flag;
} MISCOPT;

int main(int argc, char* argv[]){
	/* 照理說是 zh_TW.utf8，但考慮作業系統差異還是從環境變數抓
	 * （反正出錯是使用者的事） */
	setlocale(LC_ALL, ""); 

	int i, j, k;
	int verbose = 0; /* 0 是安靜模式（機器模式）、1 是 verbose、2 是 debug */
	int ui = SCTJUDGE_UI_CLI;
	_Bool parseok = 0;
	char* tmpstr;
	JUDGEINFO sjopt; /* 無設定檔模式下，傳送這個給 CliMain() */
	L4QARG* qarglist;

#ifdef HAVE_CONF_UGIDNAME
	struct passwd* pwinfo;
	struct group* grinfo;
#endif

	save_uids();

#ifndef HAVE_CONF_CAP
	/* 即使有 setuid root，還是不能讓每個使用者拿到 root 權限
	 * 所以說只有在 fork 的時候才要把這個權限開起來，其他就關掉吧 */
	disable_setuid();
#endif

	/* 定義 -m 時的選項清單
	 * 當然有些特殊的像是 errfile 和 uid/gid 以及 arg 這裡沒辦法 */
	const MISCOPT miscopt[] = {
	{"time", &(sjopt.procinfo.i_limit_time), 0},
	{"timelimit", &(sjopt.procinfo.i_limit_time), 0},
	{"time-limit", &(sjopt.procinfo.i_limit_time), 0},
	{"tle", &(sjopt.procinfo.i_limit_time), 0},

	{"stderr", NULL, PROCINFO_FLAG_REDIR_STDERR},
	{"redir-stderr", NULL, PROCINFO_FLAG_REDIR_STDERR},

	{"secure", NULL, PROCINFO_FLAG_SECURE_CHECK},
	{"secure-check", NULL, PROCINFO_FLAG_SECURE_CHECK},

	{"mem", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},
	{"memory", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},
	{"ram", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},
	{"as", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},
	{"addr-space", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},
	{"address-space", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},
	{"vm", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},
	{"virtual-memory", &(sjopt.procinfo.i_limit_mem), PROCINFO_FLAG_LIMIT_MEM},

	{"core", &(sjopt.procinfo.i_limit_core), PROCINFO_FLAG_LIMIT_CORE},
	{"coredump", &(sjopt.procinfo.i_limit_core), PROCINFO_FLAG_LIMIT_CORE},
	{"corefile", &(sjopt.procinfo.i_limit_core), PROCINFO_FLAG_LIMIT_CORE},

	{"cputime", &(sjopt.procinfo.i_limit_cpu), PROCINFO_FLAG_LIMIT_CPU},
	{"cputime-limit", &(sjopt.procinfo.i_limit_cpu), PROCINFO_FLAG_LIMIT_CPU},

	{"outlimit", &(sjopt.procinfo.i_limit_out), PROCINFO_FLAG_LIMIT_OUT},
	{"out-limit", &(sjopt.procinfo.i_limit_out), PROCINFO_FLAG_LIMIT_OUT},
	{"output-limit", &(sjopt.procinfo.i_limit_out), PROCINFO_FLAG_LIMIT_OUT},

	{"openfile", &(sjopt.procinfo.i_limit_file), PROCINFO_FLAG_LIMIT_FILE},
	{"fdlimit", &(sjopt.procinfo.i_limit_file), PROCINFO_FLAG_LIMIT_FILE},
	{"fdmax", &(sjopt.procinfo.i_limit_file), PROCINFO_FLAG_LIMIT_FILE},
	{"filelimit", &(sjopt.procinfo.i_limit_file), PROCINFO_FLAG_LIMIT_FILE},
	{"file-limit", &(sjopt.procinfo.i_limit_file), PROCINFO_FLAG_LIMIT_FILE},

	{"process", &(sjopt.procinfo.i_limit_proc), PROCINFO_FLAG_LIMIT_PROC},
	{"proc", &(sjopt.procinfo.i_limit_proc), PROCINFO_FLAG_LIMIT_PROC},
	{"proc-limit", &(sjopt.procinfo.i_limit_proc), PROCINFO_FLAG_LIMIT_PROC},

	{"stack", &(sjopt.procinfo.i_limit_stack), PROCINFO_FLAG_LIMIT_STACK}, 
	{"stack-size", &(sjopt.procinfo.i_limit_stack), PROCINFO_FLAG_LIMIT_STACK}, 
	{"stack-limit", &(sjopt.procinfo.i_limit_stack), PROCINFO_FLAG_LIMIT_STACK}

	};



	/* 將無設定檔模式的結構設定為預設值 */
	sctjudge_setdefault(&sjopt);
	sjopt.procinfo.i_argv = l4da_create(sizeof(char*), 2);
	tmpstr = NULL;
	l4da_pushback(sjopt.procinfo.i_argv, &tmpstr);

	/* 解析命令列的時間到了
	 * 為了方便與安全起見，所有 sjopt 裡面的字串都會用 strdup() 複製一次 */
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
					WITH_NULL "\n\n"
					"  -o/-output <檔案>\n"
					"      指定要導向至受測程式標準輸出的檔案，若未指定則為 "
					WITH_NULL "\n\n"
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
					WITH_NULL "\n\n"
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
				return SCTJUDGE_EXIT_SUCCESS;

			}else if(!strcmp(&argv[i][1], "V") ||
					!strcmp(&argv[i][1], "version") ||
					!strcmp(&argv[i][1], "-version")){
				fputs(SCTJUDGE_ABOUT_STRING, stdout);
				exit(SCTJUDGE_EXIT_SUCCESS);

			}else if(!strcmp(&argv[i][1], "v") || 
					!strcmp(&argv[i][1], "verbose")){
				verbose++;
				sjopt.procinfo.i_verbose = verbose;

			}else if(!strcmp(&argv[i][1], "d") || 
					!strcmp(&argv[i][1], "debug")){
				verbose += 2;
				sjopt.procinfo.i_verbose = verbose;

			}else if(!strcmp(&argv[i][1], "q") || 
					!strcmp(&argv[i][1], "quiet") ||
					!strcmp(&argv[i][1], "silent")){
				verbose = 0;
				sjopt.procinfo.i_verbose = verbose;

			}else if(!strcmp(&argv[i][1], "n") || 
					!strcmp(&argv[i][1], "dryrun")){
				verbose = (verbose < 1) ? 1 : verbose;
				sjopt.procinfo.i_flag |= PROCINFO_FLAG_DRYRUN;

			}else if(!strcmp(&argv[i][1], "f") ||
					!strcmp(&argv[i][1], "force")){
				sjopt.procinfo.i_flag |= PROCINFO_FLAG_FORCE;

			}else if(!strcmp(&argv[i][1], "t") ||
					!strcmp(&argv[i][1], "time")){
				SCTJUDGE_CLIARG_CHECK_MISSING;
				if(str_to_u(argv[i], &sjopt.procinfo.i_limit_time) < 0){
					fprintf(stderr, "%s: 「%s」不是正確的時間設定值\n", 
							argv[0], argv[i]);
					exit(SCTJUDGE_EXIT_SYNTAX);
				}

			}else if(!strcmp(&argv[i][1], "e") ||
					!strcmp(&argv[i][1], "exec")){
				SCTJUDGE_CLIARG_CHECK_MISSING;
				FREE_IF_NOT_NULL(sjopt.procinfo.i_exec);
				sjopt.procinfo.i_exec = strdup(argv[i]);

			}else if(!strcmp(&argv[i][1], "i") ||
					!strcmp(&argv[i][1], "in") ||
					!strcmp(&argv[i][1], "input")){
				SCTJUDGE_CLIARG_CHECK_MISSING;
				FREE_IF_NOT_NULL(sjopt.procinfo.i_infile);
				sjopt.procinfo.i_infile = strdup(argv[i]);

			}else if(!strcmp(&argv[i][1], "o") ||
					!strcmp(&argv[i][1], "out") ||
					!strcmp(&argv[i][1], "output")){
				SCTJUDGE_CLIARG_CHECK_MISSING;
				FREE_IF_NOT_NULL(sjopt.procinfo.i_outfile);
				sjopt.procinfo.i_outfile = strdup(argv[i]);

			}else if(!strcmp(&argv[i][1], "r") ||
					!strcmp(&argv[i][1], "root") ||
					!strcmp(&argv[i][1], "chroot")){
				SCTJUDGE_CLIARG_CHECK_MISSING;
				FREE_IF_NOT_NULL(sjopt.procinfo.i_chroot);
				sjopt.procinfo.i_chroot = strdup(argv[i]);

			}else if(!strcmp(&argv[i][1], "u") ||
					!strcmp(&argv[i][1], "ui") ||
					!strcmp(&argv[i][1], "interface")){
				SCTJUDGE_CLIARG_CHECK_MISSING;
				if(!strcmp(argv[i], "c") ||
					!strcmp(argv[i], "cli") ||
					!strcmp(argv[i], "cmd") ||
					!strcmp(argv[i], "cmdln") ||
					!strcmp(argv[i], "command") ||
					!strcmp(argv[i], "commandline")){
					ui = SCTJUDGE_UI_CLI;
				}else if(!strcmp(argv[i], "t") ||
					!strcmp(argv[i], "tui") ||
					!strcmp(argv[i], "text") ||
					!strcmp(argv[i], "term") ||
					!strcmp(argv[i], "terminal") ||
					!strcmp(argv[i], "curses") ||
					!strcmp(argv[i], "ncurses")){
					ui = SCTJUDGE_UI_TUI;
				}else if(!strcmp(argv[i], "g") ||
					!strcmp(argv[i], "gui") ||
					!strcmp(argv[i], "graph") ||
					!strcmp(argv[i], "graphic") ||
					!strcmp(argv[i], "graphics") ||
					!strcmp(argv[i], "graphical") ||
					!strcmp(argv[i], "x") ||
					!strcmp(argv[i], "xorg") ||
					!strcmp(argv[i], "xwin") ||
					!strcmp(argv[i], "xwindow")){
					ui = SCTJUDGE_UI_GUI;
				}else{
					fprintf(stderr, "%s: 不明的使用者界面「%s」\n",
							argv[0], argv[i]);
					exit(SCTJUDGE_EXIT_SYNTAX);
				}

			}else if(!strcmp(&argv[i][1], "m") ||
					!strcmp(&argv[i][1], "misc") ||
					!strcmp(&argv[i][1], "opt") ||
					!strcmp(&argv[i][1], "option")){
				SCTJUDGE_CLIARG_CHECK_MISSING;
				qarglist = l4qarg_parse(argv[i]);
				for(j=0; !l4qarg_end(qarglist[j]); j++){
					parseok = 0;
					for(k=0; k < sizeof(miscopt) / sizeof(MISCOPT); k++){
						if(!strcmp(miscopt[k].id, qarglist[j].arg_name)){
							if(miscopt[k].num == NULL){
								SCTJUDGE_CLIARG_CHECK_QARG_EXTRA;
								sjopt.procinfo.i_flag |= miscopt[k].flag;
								parseok = 1;
								break;
							}else{
								SCTJUDGE_CLIARG_CHECK_QARG_MISSING;
								sjopt.procinfo.i_flag |= miscopt[k].flag;
								if(str_to_u(
									qarglist[j].arg_value, miscopt[k].num) < 0)
								{
									fprintf(stderr, "%s: 「%s」並不是整數\n",
										argv[0], qarglist[j].arg_value);
									exit(SCTJUDGE_EXIT_SYNTAX);
								}
								parseok = 1;
								break;
							}
						}

						/* 這裡是有加上 no 的選項 */
						if(strlen(qarglist[j].arg_name) >= 2 &&
							!strncmp(qarglist[j].arg_name, "no", 2) &&
							!strcmp((qarglist[j].arg_name)+2, miscopt[k].id)){
							SCTJUDGE_CLIARG_CHECK_QARG_EXTRA;
							sjopt.procinfo.i_flag &= ~(miscopt[k].flag);
							parseok = 1;
							break;
						}

					}

					if(parseok){
						continue;
					}

					/* copy/nocopy 設在 JUDGEINFO 但不在 PROCINFO */
					if(!strcmp(qarglist[j].arg_name, "copy")){
						SCTJUDGE_CLIARG_CHECK_QARG_EXTRA;
						sjopt.flag |= JUDGEINFO_FLAG_COPY;
					}else if(!strcmp(qarglist[j].arg_name, "nocopy")){
						SCTJUDGE_CLIARG_CHECK_QARG_EXTRA;
						sjopt.flag &= ~(JUDGEINFO_FLAG_COPY);
					}
					
					/* 修改 uid / gid 的功能 */
					else if(!strcmp(qarglist[j].arg_name, "uid") ||
							!strcmp(qarglist[j].arg_name, "user")){
						SCTJUDGE_CLIARG_CHECK_QARG_MISSING;
						sjopt.procinfo.i_flag |= PROCINFO_FLAG_SETUID;
						if(str_to_u(
							qarglist[j].arg_value, &sjopt.procinfo.i_uid) < 0){
#ifdef HAVE_CONF_UGIDNAME
							pwinfo = getpwnam(qarglist[j].arg_value);
							if(pwinfo == NULL){
								fprintf(stderr, "%s: 「%s」不是正確的 UID 或"
										"使用者名稱\n", 
										argv[0], qarglist[j].arg_value);
								exit(SCTJUDGE_EXIT_SYNTAX);
							}
							sjopt.procinfo.i_uid = pwinfo->pw_uid;
#else
							fprintf(stderr, "%s: 「%s」並不是整數\n",
									argv[0], qarglist[j].arg_value);
							exit(SCTJUDGE_EXIT_SYNTAX);
#endif
						}
					}else if(!strcmp(qarglist[j].arg_name, "gid") ||
							!strcmp(qarglist[j].arg_name, "group")){
						SCTJUDGE_CLIARG_CHECK_QARG_MISSING;
						sjopt.procinfo.i_flag |= PROCINFO_FLAG_SETGID;
						if(str_to_u(
							qarglist[j].arg_value, &sjopt.procinfo.i_gid) < 0){
#ifdef HAVE_CONF_UGIDNAME
							grinfo = getgrnam(qarglist[j].arg_value);
							if(grinfo == NULL){
								fprintf(stderr, "%s: 「%s」不是正確的 GID 或"
										"群組名稱\n", 
										argv[0], qarglist[j].arg_value);
								exit(SCTJUDGE_EXIT_SYNTAX);
							}
							sjopt.procinfo.i_gid = grinfo->gr_gid;
#else
							fprintf(stderr, "%s: 「%s」並不是整數\n",
									argv[0], qarglist[j].arg_value);
							exit(SCTJUDGE_EXIT_SYNTAX);
#endif
						}
					}
					
					/* stderr 也可以指定個別的檔案 */
					else if(!strcmp(qarglist[j].arg_name, "err") ||
							!strcmp(qarglist[j].arg_name, "errfile")){
						SCTJUDGE_CLIARG_CHECK_QARG_MISSING;
						FREE_IF_NOT_NULL(sjopt.procinfo.i_errfile);
						sjopt.procinfo.i_errfile = 
							strdup(qarglist[j].arg_value);
					}
					
					/* 可以指定程式執行時的參數 */
					else if(!strcmp(qarglist[j].arg_name, "arg") ||
							!strcmp(qarglist[j].arg_name, "argv") ||
							!strcmp(qarglist[j].arg_name, "addarg") ||
							!strcmp(qarglist[j].arg_name, "appendarg")){
						SCTJUDGE_CLIARG_CHECK_QARG_MISSING;
						tmpstr = strdup(qarglist[j].arg_value);
						l4da_pushback(sjopt.procinfo.i_argv, &tmpstr);
					}else if(!strcmp(qarglist[j].arg_name, "arg0") ||
							!strcmp(qarglist[j].arg_name, "argv0")){
						SCTJUDGE_CLIARG_CHECK_QARG_MISSING;
						tmpstr = strdup(qarglist[j].arg_value);
						l4da_v(sjopt.procinfo.i_argv, char*, 0) = tmpstr;
					}
					else{
						fprintf(stderr, "%s: 「%s」是不明的選項\n", argv[0],
								qarglist[j].arg_name);
						exit(SCTJUDGE_EXIT_SYNTAX);
					}
				}
				l4qarg_free(qarglist);
			}else{
				fprintf(stderr, "%s: 不明的選項「%s」\n"
					"請嘗試執行 %s -help 來取得說明\n"
					, argv[0], argv[i], argv[0]);
				return SCTJUDGE_EXIT_SYNTAX;
			}
		}else{
			fprintf(stderr, "%s: 抱歉，設定檔功能尚未實作\n", argv[0]);
			return SCTJUDGE_EXIT_NA;
		}
	}

	if(l4da_v(sjopt.procinfo.i_argv, char*, 0) == NULL){
		/* 未明確指定 argv[0] 則直接將執行檔名稱複製過去 */
		if(sjopt.procinfo.i_exec != NULL){
			l4da_v(sjopt.procinfo.i_argv, char*, 0) = 
				strdup(sjopt.procinfo.i_exec);
		}
	}

	/* argv 尾端必須是 NULL */
	tmpstr = NULL;
	l4da_pushback(sjopt.procinfo.i_argv, &tmpstr);

	/* 至此可以進入主程式了 */
	if(verbose >= 1){
		puts(SCTJUDGE_TITLEBAR"\n");
	}

	switch(ui){
		case SCTJUDGE_UI_CLI:
			if(verbose >= 2){
				printf("DEBUG: %s: Call sctcli_main() [Entering CLI mode]\n", 
						__func__);
			}
			sctcli_main(NULL, &sjopt);
			break;

		case SCTJUDGE_UI_TUI:
			fprintf(stderr, "%s: 抱歉，curses 界面尚未實作\n", argv[0]);
			exit(SCTJUDGE_EXIT_NA);
			break;

		case SCTJUDGE_UI_GUI:
			fprintf(stderr, "%s: 抱歉，圖形化使用者界面尚未實作\n", argv[0]);
			exit(SCTJUDGE_EXIT_NA);
			break;

		default:
			fprintf(stderr, "%s: internal error: invalid ui %d\n",
					argv[0], ui);
			exit(SCTJUDGE_EXIT_NA);
	}

	sctjudge_freestring(&sjopt);
	
	/**********************************************************************
	 * 以下都要清掉！！！
	 * ********************************************************************/

#if 0
	/* 檢查與修正設定值 */
	if(verbose){
		puts("正在檢查設定值是否正確......");
	}
	if(mcopt.executable == NULL){
		fputs("受測程式可執行檔名稱為必要參數，不可為空白\n", stderr);
		exit(SCTEXIT_TOOFEW);
	}
	if(mcopt.inputfile == NULL){
		mcopt.inputfile = WITH_NULL;
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
				if(procrealuid != 0){
					fputs("只有 root 可以將 UID 設定 0\n", stderr);
					exit(SCTEXIT_BADID);
				}
			}
		}
	}else{
		if(procrealuid == 0 && !force){
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
	sem_init(&tlethr, 0, 0);
	sem_init(&dispthr, 0, 0);
	pthread_create(&tmain, &joistate, &sctjudge_makechild, (void*)&mcopt);
	if(!dryrun){
		pthread_create(&tkill, &detstate, &sctjudge_checktle, 
				(void*)&exectimelimit);
	}
	if(!dryrun && verbose){
		pthread_create(&tdisplay, &detstate, &sctjudge_displaytime,
				(void*)&displayinterval);
	}
	pthread_attr_destroy(&detstate);
	pthread_attr_destroy(&joistate);
	sem_post(&mcthr); 

	pthread_join(tmain, (void**)&mtreturn);
	/* XXX 底下這訊息很有可能讓使用者覺得很奇怪 */
	pthread_mutex_destroy(&pidmutex);
	pthread_mutex_destroy(&tkill_mx);
	pthread_mutex_destroy(&tdisplay_mx);

	sem_destroy(&mcthr);
	sem_destroy(&tlethr);
	sem_destroy(&dispthr);

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

#endif
	return SCTJUDGE_EXIT_SUCCESS;
}
