/* 你沒有看錯，真的有 config2.h
 * 這個檔案用來存放人家所說的「程式內定」的常數 (hard coding) */

/* 丟給 nanosleep 用的 */
#define SCT_CHECKTLE_INTERVAL 128000000
#define SCT_DISPTIME_INTERVAL 384000000

/* sctjudge 這個程式的 exit status */
#define SCTJUDGE_EXIT_SUCCESS 0  /* 當然就是正常結束 */
#define SCTJUDGE_EXIT_SYNTAX  1  /* 命令列語法錯誤 */
#define SCTJUDGE_EXIT_TOOFEW  2  /* 少了一些必須的選項 */
#define SCTJUDGE_EXIT_THREAD  3  /* pthread_create 以後才發生錯誤 */
