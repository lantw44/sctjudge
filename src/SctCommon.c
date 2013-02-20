#ifdef HAVE_CONFIG_H
# include "SctConfig.h"
#endif

#include "SctCommon.h"

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <pthread.h>

uid_t procrealuid = 0;
uid_t proceffuid = 0;

int str_to_u(const char* s, unsigned* u){
	char* p;
	unsigned o;
	errno = 0;
	o = strtoul(s, &p, 0);
	if(errno || *p){
		return -1;
	}
	*u = o;
	return 0;
}

int read_complete(int fd, void* buf, size_t count){
	/* 一定要讀到 count 位元組的資料才會回傳，所以只要回傳 < count
	 * 就表示已經讀到 EOF 了 */
	char* bufp = (char*)buf;
	size_t curcount = 0;
	int rval;
	do{
		rval = read(fd, bufp, count - curcount);
		if(rval < 0){
			return rval;
		}else if(rval == 0){
			return curcount;
		}else{
			bufp += rval;
			curcount += rval;
		}
	}while(curcount < count);
	return count;
}

int strerror_threadsafe(int errnum, char* buf, size_t buflen){
	static pthread_mutex_t strerror_call_mutex = PTHREAD_MUTEX_INITIALIZER;
	char* rval;
	int rlen;
	pthread_mutex_lock(&strerror_call_mutex);
	rval = strerror(errnum);
	rlen = strlen(rval);
	if(buflen < rlen){
		pthread_mutex_unlock(&strerror_call_mutex	);
		return -1;
	}
	strcpy(buf, rval);
	pthread_mutex_unlock(&strerror_call_mutex);
	return 0;
}

int sprintf_malloc(char** strstore, const char* format, ...){
	int len;
	char* newstr;
	va_list ap;

	va_start(ap, format);
	len = vsnprintf(NULL, 0, format, ap) + 1;
	va_end(ap);

	newstr = malloc(len);
	if(newstr == NULL){
		return -1;
	}

	va_start(ap, format);
	len = vsnprintf(newstr, len, format, ap);
	va_end(ap);

	*strstore = newstr;
	return len;
}

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

void save_uids(void){	/* 這個一定要先執行，不然預設 uid 就是 0 */
	procrealuid = getuid();
	proceffuid = geteuid();
}

#ifndef HAVE_CONF_CAP

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
