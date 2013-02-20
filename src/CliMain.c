#ifdef HAVE_CONFIG_H
# include "SctConfig.h"
#endif

#include "SctVersion.h"
#include "ProcCommon.h"
#include "JudgeCommon.h"
#include "CliCommon.h"

#include <stdio.h>
#include <stdarg.h>

static int sctcli_verbose;

/* TODO: 實作 configfile 的功能 */
void sctcli_main(void* configfile, JUDGEINFO* sjdata){
	sctcli_verbose = sjdata->procinfo.i_verbose;
	sjdata->procinfo.i_func_arg = NULL;
	sjdata->procinfo.f_debug = sctcli_debug_backend;
	sjdata->procinfo.f_normal = sctcli_normal_backend;
	sjdata->procinfo.f_err = sctcli_err_backend;
	sjdata->procinfo.f_monitor = sctcli_monitor;
	if(configfile == NULL){
		sctcli_logger(1, "開始執行檢測工作......\n");
		sctcli_logger(2, "%s: Call sctjudge_main() [Judge function]\n",
				__func__);
		if(sctjudge_main(sjdata) < 0){ /* 執行檢測工作 */
			sctcli_err(NULL, "檢測工作失敗");
			return;
		}

	}else{
		sctcli_err(NULL, "抱歉，設定檔功能尚未實作\n");
		return;
	}
}

void sctcli_logger(int level, const char* format, ...){
	va_list ap;
	va_start(ap, format);
	if(sctcli_verbose >= level){
		if(level >= 2){
			sctcli_debug_backend(NULL, format, ap);
		}else{
			sctcli_normal_backend(NULL, format, ap);
		}
	}
	va_end(ap);
}

void sctcli_normal_backend(void* data, const char* format, va_list varlist){
	vfprintf(stdout, format, varlist);
}

void sctcli_debug_backend(void* data, const char* format, va_list varlist){
	fputs("DEBUG: ", stdout);
	vfprintf(stdout, format, varlist);
}

void sctcli_err_backend(void* data, const char* format, va_list varlist){
	fputs("錯誤: ", stderr);
	vfprintf(stderr, format, varlist);
}

void sctcli_err(const char* format, ...){
	va_list ap;
	va_start(ap, format);
	sctcli_err_backend(NULL, format, ap);
	va_end(ap);
}

void sctcli_monitor(void* data, const PROCMONINFO* moninfo){
	return;
}
