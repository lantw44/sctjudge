#ifndef SCTJUDGE_CLI_COMMON
#define SCTJUDGE_CLI_COMMON

#include "ProcCommon.h"
#include "JudgeCommon.h"

#include <stdarg.h>

/* --- CliMain.c --- */
void sctcli_main(void*, JUDGEINFO*);
void sctcli_logger(int, const char*, ...);
void sctcli_err(const char*, ...);
void sctcli_normal_backend(void*, const char*, va_list);
void sctcli_debug_backend(void*, const char*, va_list);
void sctcli_err_backend(void*, const char*, va_list);
void sctcli_monitor(void*, const PROCMONINFO*);

#endif
