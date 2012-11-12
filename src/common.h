#ifndef SCTJUDGE_COMMON_FUNCTIONS
#define SCTJUDGE_COMMON_FUNCTIONS

#include <time.h>
#include <unistd.h>
#include <sys/types.h>

void checktimespec(struct timespec*);
void difftimespec(const struct timespec*, const struct timespec*,
		struct timespec*);
int comparetimespec(const struct timespec*, const struct timespec*);
void save_uids(void);    /* 這個一定要先執行，不然預設 uid 就是 0 */

#ifndef HAVE_CONF_CAP
void disable_setuid(void);
void enable_setuid(void);
#endif

/* 避免 disable_setuid() 之後 getuid() 得到錯誤的值，所以用這些變數把最初的
 * UID 給記下來！*/
extern uid_t procrealuid;
extern uid_t proceffuid;

#endif
