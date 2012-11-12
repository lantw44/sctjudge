#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "common.h"

#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef HAVE_CONF_CAP
uid_t procrealuid = 0;
uid_t proceffuid = 0;
#endif

void checktimespec(struct timespec* arg){
	long tominus;
	if(arg->tv_nsec >= 1000000000){
		arg->tv_sec += (arg->tv_nsec / 1000000000);
		arg->tv_nsec %= 1000000000;
	}else if(arg->tv_nsec < 0){
		if((-(arg->tv_nsec)) % 1000000000 == 0){
			arg->tv_sec -= ((-(arg->tv_nsec)) / 1000000000);
			arg->tv_nsec = 0;
		}else{
			tominus = (((-(arg->tv_nsec)) / 1000000000) + 1);
			arg->tv_sec -= tominus;
			arg->tv_nsec += tominus * 1000000000;
		}
	}
}

void difftimespec(start, end, out)
	const struct timespec* start;
	const struct timespec* end;
	struct timespec* out;
{
	out->tv_sec = end->tv_sec - start->tv_sec;
	out->tv_nsec = end->tv_nsec - start->tv_nsec;
	if(out->tv_nsec < 0){
		out->tv_nsec += 1000000000;
		out->tv_sec--;
	}
}

int comparetimespec(t1, t2)
	const struct timespec* t1;
	const struct timespec* t2;
{
	if(t1->tv_sec < t2->tv_sec){
		return -1;
	}else if(t1->tv_sec > t2->tv_sec){
		return 1;
	}else{
		if(t1->tv_nsec < t2->tv_nsec){
			return -1;
		}else if(t1->tv_nsec > t2->tv_nsec){
			return 1;
		}else{
			return 0;
		}
	}
	return 0;
}

#ifndef HAVE_CONF_CAP

void save_uids(void){	/* 這個一定要先執行，不然預設 uid 就是 0 */
	procrealuid = getuid();
	proceffuid = geteuid();
}

void disable_setuid(void){
#ifdef _POSIX_SAVED_IDS
	seteuid(procrealuid);
#else
	setreuid(proceffuid, procrealuid);
#endif
}

void enable_setuid(void){
#ifdef _POSIX_SAVED_IDS
	seteuid(proceffuid);
#else
	setreuid(procrealuid, proceffuid);
#endif
}

#endif
