#ifndef SCTJUDGE_SCT_COMMON
#define SCTJUDGE_SCT_COMMON

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

/* --- SctCommon.c --- */

/* 字串轉整數 wrapper */
int str_to_u(const char*, unsigned*);

/* 一定要讀到 count 位元組的資料才會回傳，所以只要回傳 < count
 * 就表示已經讀到 EOF 了 */
int read_complete(int fd, void* buf, size_t count);



/* 安全的 strerror */
int strerror_threadsafe(int, char*, size_t);

/* 會自動配置空間的 sprintf */
int sprintf_malloc(char**, const char*, ...);


/* 將某個 timespec 調整成合理的數值 */
void checktimespec(struct timespec*);

/* 計算前兩個 timespec 的數值差異，存到第三個參數 */
void difftimespec(const struct timespec*, const struct timespec*,
		struct timespec*);

/* 比較兩個 timespec 的大小，使用方法與 strcmp 相同 */
int comparetimespec(const struct timespec*, const struct timespec*);



/* 這個一定要先執行，不然預設 uid 就是 0 
 * 注意這函式只應該在 main() 一開始執行，不然就不知道最初的 uid 了！ */
void save_uids(void);    

#ifndef HAVE_CONF_CAP
void disable_setuid(void);
void enable_setuid(void);
#endif

/* 避免 disable_setuid() 之後 getuid() 得到錯誤的值，所以用這些變數把最初的
 * UID 給記下來！*/
extern uid_t procrealuid;
extern uid_t proceffuid;





/* --- SctMain.c --- */

/* 如果不是 NULL 就 free 掉它 */
#define FREE_IF_NOT_NULL(p) \
	if((p) != NULL){ \
		free(p); \
	} \
	(p) = NULL

/* 如果是 NULL 就替換成 "(NULL)" */
#define STRING_NULL(p) \
	((p) == NULL) ? ("(NULL)") : (p)

/* 測試 flag 是 yes 還是 no 並轉成字串 */
#define DISPLAY_YESNO(flag, value) \
	(((flag) & (value)) ? ("Yes") : ("No"))

/* sctjudge 的使用者界面選擇 */
#define SCTJUDGE_UI_CLI 0
#define SCTJUDGE_UI_TUI 1
#define SCTJUDGE_UI_GUI 2

/* sctjudge 這個程式的 exit status 
 * 注意，檢測 malloc 失敗的東西已經去掉了 */
#define SCTJUDGE_EXIT_SUCCESS   0  /* 當然就是正常結束 */
#define SCTJUDGE_EXIT_SYNTAX    1  /* 解析階段：命令列語法錯誤 */
#define SCTJUDGE_EXIT_INVALID   2  /* 配置無效或需要強制執行 */
#define SCTJUDGE_EXIT_INSECURE  3  /* 不安全的設定值 */
#define SCTJUDGE_EXIT_REJECT    4  /* 拒絕此參數或需要強制執行 */
#define SCTJUDGE_EXIT_NA      255  /* 尚未實作的功能 */

#endif
