#ifndef SCTJUDGE_COMMON_FUNCTIONS
#define SCTJUDGE_COMMON_FUNCTIONS

#include <time.h>
#include <unistd.h>
#include <sys/types.h>

void checktimespec(struct timespec*);
void difftimespec(const struct timespec*, const struct timespec*,
		struct timespec*);
int comparetimespec(const struct timespec*, const struct timespec*);


#ifndef HAVE_CONF_CAP
void save_uids(void);    /* 這個一定要先執行，不然預設 uid 就是 0 */
void disable_setuid(void);
void enable_setuid(void);
#endif


#endif
