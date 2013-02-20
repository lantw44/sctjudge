#ifdef HAVE_CONFIG_H
# include "SctConfig.h"
#endif

#include "SctCommon.h"
#include "SctVersion.h"
#include "JudgeCommon.h"
#include <stdarg.h>

const char* sctjudge_result_text[JUDGEINFO_RESULT_MAX][2] = {
	{"OK" , "正常結束"},
	{"RE" , "執行過程中發生錯誤"},
	{"TLE", "超過時間限制"},
	{"OLE", "輸出超過限制"},
	{"SLE", "暫停次數太多"},
	{"AB" , "使用者中斷操作"},
	{"UD" , "未定義的結果"},
	{"WA" , "錯誤的答案"},
	{"PE" , "程式大致正確但輸出格式不符"},
	{"AC" , "已通過檢測"}
};

static void (*sctjudge_output_debug)(void*, const char*, va_list);
static void (*sctjudge_output_normal)(void*, const char*, va_list);
static void (*sctjudge_output_err)(void*, const char*, va_list);
static void* sctjudge_output_arg;
static int sctjudge_verbose;

int sctjudge_main(JUDGEINFO* judgeinfo){
	sctjudge_output_debug = judgeinfo->procinfo.f_debug;
	sctjudge_output_normal = judgeinfo->procinfo.f_normal;
	sctjudge_output_err = judgeinfo->procinfo.f_err;
	sctjudge_output_arg = judgeinfo->procinfo.i_func_arg;
	sctjudge_verbose = judgeinfo->procinfo.i_verbose;

	/* 列出設定值，僅供偵錯使用 */
	sctjudge_logger(2, "%s: 列出 JUDGEINFO 中的設定值\n", __func__);
	sctjudge_logger(2, "%s: flag: COPY [自動複製可執行檔]: %s\n", __func__,
		DISPLAY_YESNO(judgeinfo->flag, JUDGEINFO_FLAG_COPY));
	sctjudge_logger(2, "%s: petect: 此功能尚未實作\n", __func__);

	int rval;

	/* 準備執行程式 */
	sctjudge_logger(1, "開始執行受測程式......\n");
	sctjudge_logger(2, "%s: Call sctproc_main() [Process creator]\n", __func__);
	if((rval = sctproc_main(&(judgeinfo->procinfo)))){
		sctjudge_err("受測程式執行失敗\n");
		return rval;
	}
	return 0;
}

void sctjudge_setdefault(JUDGEINFO* judgeinfo){
	sctproc_setdefault(&(judgeinfo->procinfo));
	judgeinfo->procmain = 0;
	judgeinfo->pedetect = 0;
	judgeinfo->result = 0;
	judgeinfo->flag = 
		JUDGEINFO_FLAG_COPY;
}

void sctjudge_freestring(JUDGEINFO* judgeinfo){
	sctproc_freestring(&(judgeinfo->procinfo));
}

void sctjudge_logger(int level, const char* format, ...){
	va_list ap;
	va_start(ap, format);
	if(sctjudge_verbose >= level){
		if(level >= 2){
			(*sctjudge_output_debug)(sctjudge_output_arg, format, ap);
		}else{
			(*sctjudge_output_normal)(sctjudge_output_arg, format, ap);
		}
	}
	va_end(ap);
}

void sctjudge_err(const char* format, ...){
	va_list ap;
	va_start(ap, format);
	(*sctjudge_output_err)(sctjudge_output_arg, format, ap);
	va_end(ap);
}
