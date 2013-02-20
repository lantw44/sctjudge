#ifdef HAVE_CONFIG_H
# include "SctConfig.h"
#endif

#include "SctConst.h"
#include "SctCommon.h"
#include "ProcCommon.h"

#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <pthread.h>
#include <semaphore.h>

volatile sig_atomic_t sctproc_abort;

static void break_handler(int signo){
	sctproc_abort = 1;
	sem_post(&checktime_sem);
}

void* sctproc_checktime(void* arg){
	PROCINFO* procinfo = arg;
	struct sigaction break_catch;
	struct timespec timelimit, timeinit, timecur, timeexpire;
	long long sleeptime = (long long)(procinfo->i_limit_time) * 1000000;

	pthread_mutex_lock(&checktime_mutex);
	checktime_exist = 1;
	pthread_mutex_unlock(&checktime_mutex);

	clock_gettime(CLOCK_REALTIME, &timeinit);

#ifndef HAVE_CONF_CAP
	enable_setuid();
#endif

	sctproc_abort = 0;
	memset(&break_catch, 0, sizeof(break_catch));
	break_catch.sa_handler = &break_handler;
	sigaction(SIGINT, &break_catch, NULL);
	sigaction(SIGTERM, &break_catch, NULL);

	timelimit.tv_sec = timeinit.tv_sec + sleeptime / 1000000000;
	timelimit.tv_nsec = timeinit.tv_nsec + sleeptime % 1000000000;
	checktimespec(&timelimit);

	do{
		clock_gettime(CLOCK_REALTIME, &timecur);
		timeexpire.tv_sec = timecur.tv_sec;
		timeexpire.tv_nsec = timecur.tv_nsec + SCTJUDGE_PROC_CHECKTIME_INTERVAL;
		checktimespec(&timeexpire);
	}while(comparetimespec(&timecur, &timelimit) < 0 && 
		sem_timedwait(&checktime_sem, &timeexpire));

	if(!sctproc_abort){
		pthread_mutex_lock(&sctproc_tle_mutex);
		sctproc_tle = 1;
		pthread_mutex_unlock(&sctproc_tle_mutex);
	}

	kill(child_pid, SIGKILL);

	pthread_mutex_lock(&checktime_mutex);
	checktime_exist = 0;
	pthread_mutex_unlock(&checktime_mutex);
	return NULL;
}

