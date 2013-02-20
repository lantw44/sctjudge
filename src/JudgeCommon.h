#ifndef SCTJUDGE_JUDGE_COMMON
#define SCTJUDGE_JUDGE_COMMON

#include "ProcCommon.h"

/* 這裡的東西記得和 ProcCommon.h 與 SctMain.c 連動 */
#define JUDGEINFO_RESULT_OK    PROCINFO_RESULT_OK
#define JUDGEINFO_RESULT_RE    PROCINFO_RESULT_RE  
#define JUDGEINFO_RESULT_TLE   PROCINFO_RESULT_TLE
#define JUDGEINFO_RESULT_OLE   PROCINFO_RESULT_OLE
#define JUDGEINFO_RESULT_SLE   PROCINFO_RESULT_SLE
#define JUDGEINFO_RESULT_AB    PROCINFO_RESULT_AB
#define JUDGEINFO_RESULT_UD    PROCINFO_RESULT_UD
#define JUDGEINFO_RESULT_WA    (PROCINFO_RESULT_MAX + 0)
#define JUDGEINFO_RESULT_PE    (PROCINFO_RESULT_MAX + 1)
#define JUDGEINFO_RESULT_AC    (PROCINFO_RESULT_MAX + 2)
#define JUDGEINFO_RESULT_MAX   (PROCINFO_RESULT_MAX + 3)

/* --- JudgeMain.c --- */

/* 全域常數，表示 JUDGEINFO 中 result 的值 */
extern const char* sctjudge_result_text[][2];
#define JUDGEINFO_GET_RESULT_ABBR(n) (sctjudge_result_text[(n)][0])
#define JUDGEINFO_GET_RESULT_TEXT(n) (sctjudge_result_text[(n)][1])

typedef struct sctjudge_judgeinfo{
	PROCINFO procinfo;  /* 交給 Proc 系列函式的參數和結果 */
	int procmain;       /* ProcMain 回傳的東西 */
	int flag;           /* 與 Judge 相關的參數，但不會傳到 Proc */
	int pedetect;       /* 偵測 PE 的方法 */
	int result;         /* Judge 的結果 */
	/* 未完待續...... (special judge ?) */
} JUDGEINFO;

/* JUDGEINFO 裡面 judgeflag 的值 */
#define JUDGEINFO_FLAG_COPY     0x00000001  /* chroot 所以要複製執行檔 */

/* sctjudge_main 的回傳值，0 表示正常 */
#define SCTJUDGE_JUDGE_EXIT_SUCCESS    0
#define SCTJUDGE_JUDGE_EXIT_MAX        (SCTJUDGE_PROC_EXIT_MAX + 0)

int sctjudge_main(JUDGEINFO*);
void sctjudge_setdefault(JUDGEINFO*);
void sctjudge_freestring(JUDGEINFO*);
void sctjudge_logger(int, const char*, ...);
void sctjudge_err(const char*, ...);

#endif
