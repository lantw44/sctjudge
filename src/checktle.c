#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "config2.h"
#include "common.h"
#include "sctcore.h"

#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <pthread.h>
#include <semaphore.h>

volatile sig_atomic_t break_flag;

static void break_handler(int signo){
	break_flag = 1;
	sem_post(&tlethr);
}

void* sctjudge_checktle(void* arg){
	pid_t pidcopy;
	long long sleeptime = (long long)(*(int*)arg) * 1000000;
	struct sigaction break_catch;
	struct timespec timelimit, timeinit, timecur, timeexpire;

	pthread_mutex_lock(&tkill_mx);
	tkill_yes = 1;
	pthread_mutex_unlock(&tkill_mx);

	sem_wait(&tlethr);
	clock_gettime(CLOCK_REALTIME, &timeinit);

#ifndef HAVE_CONF_CAP
	enable_setuid();
#endif

	break_flag = 0;
	memset(&break_catch, 0, sizeof(break_catch));
	break_catch.sa_handler = &break_handler;
	sigaction(SIGINT, &break_catch, NULL);
	sigaction(SIGTERM, &break_catch, NULL);

	pthread_mutex_lock(&pidmutex);
	pidcopy = pidchild;
	pthread_mutex_unlock(&pidmutex);

	timelimit.tv_sec = timeinit.tv_sec + sleeptime / 1000000000;
	timelimit.tv_nsec = timeinit.tv_nsec + sleeptime % 1000000000;
	checktimespec(&timelimit);

	do{
		clock_gettime(CLOCK_REALTIME, &timecur);
		timeexpire.tv_sec = timecur.tv_sec;
		timeexpire.tv_nsec = timecur.tv_nsec + SCT_CHECKTLE_INTERVAL;
		checktimespec(&timeexpire);
	}while(comparetimespec(&timecur, &timelimit) < 0 && 
		sem_timedwait(&tlethr, &timeexpire));

	if(!break_flag){
		pthread_mutex_lock(&judge_tle_mx);
		judge_tle = 1;
		pthread_mutex_unlock(&judge_tle_mx);
	}

	kill(pidcopy, SIGKILL);

	pthread_mutex_lock(&tkill_mx);
	tkill_yes = 0;
	pthread_mutex_unlock(&tkill_mx);
	return NULL;
}

