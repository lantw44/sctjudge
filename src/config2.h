/* 你沒有看錯，真的有 config2.h
 * 這個檔案用來存放人家所說的「程式內定」的常數 (hard coding) */

/* 丟給 nanosleep 用的 */
#define SCT_CHECKTLE_INTERVAL 128000000
#define SCT_DISPTIME_INTERVAL 384000000

/* XXX 這個看能不能用 GNU autotool 取代，我可不確定 /dev/null 永遠存在！*/
#define NULL_DEVICE	"/dev/null"

/* sctjudge 這個程式的 exit status */
#define SCTEXIT_SUCCESS 0  /* 當然就是正常結束 */
#define SCTEXIT_SYNTAX  1  /* 解析階段：命令列語法錯誤 */
#define SCTEXIT_TOOFEW  2  /* 檢查階段：少了一些必須的選項 */
#define SCTEXIT_BADID   3  /* 檢查階段：錯誤或不允許的 uid/gid */
#define SCTEXIT_THREAD  4  /* 實作階段：thread 回傳錯誤 */
#define SCTEXIT_MALLOC  5  /* 配置記憶體發生錯誤，這個可能發生在程式任何地方 */
