#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "config2.h"
#include "common.h"
#include "sctcore.h"

#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>

void* sctjudge_displaytime(void* arg){
	struct timespec timeinit, timecur, timepast;
	struct timespec timesleep;
	struct timespec timeexpire;

	timesleep.tv_sec = 0;
	timesleep.tv_nsec = (*(long*)arg);

#ifdef HAVE_CONF_PROCMON_LINUX
	int i;

	pid_t pidcopy;

	const char* sysstatfile = WITH_LINUXPROC"/stat";
	char statfile[25], statusfile[25];
	FILE *statp, *statusp, *sysstatp;

	int strlencount, cpuinfostore;
	char firstrun = 1, hdrstore[10];

	unsigned long cpuuser, cpusystem, precpuuser, precpusystem;
	unsigned long syscpuall, presyscpuall, diffsyscpuall;

	unsigned short res_cpuuser, res_cpusystem;
	unsigned long vmsize;
#endif

	pthread_mutex_lock(&tdisplay_mx);
	tdisplay_yes = 1;
	pthread_mutex_unlock(&tdisplay_mx);

	sem_wait(&dispthr);

#ifndef HAVE_CONF_CAP
	enable_setuid();
#endif

	clock_gettime(CLOCK_REALTIME, &timeinit);

#ifdef HAVE_CONF_PROCMON_LINUX
	pthread_mutex_lock(&pidmutex);
	pidcopy = pidchild;
	pthread_mutex_unlock(&pidmutex);
#endif

#ifdef HAVE_CONF_PROCMON_LINUX
	sprintf(statfile, WITH_LINUXPROC"/%d/stat", pidcopy);
	sprintf(statusfile, WITH_LINUXPROC"/%d/status", pidcopy);
#endif

	do{
		clock_gettime(CLOCK_REALTIME, &timecur);
		difftimespec(&timeinit, &timecur, &timepast);
		printf("\r%4ld.%03ld 秒", timepast.tv_sec,
				timepast.tv_nsec / 1000000);
#ifdef HAVE_CONF_PROCMON_LINUX
		statp = fopen(statfile, "r");
		statusp = fopen(statusfile, "r");
		sysstatp = fopen(sysstatfile, "r");
		if(statp != NULL && statusp != NULL && sysstatp != NULL){
			strlencount = 0;
			while(getc(statusp) != '\n'){
				strlencount++;
			}
			strlencount -= 8; /* 把前面的「Name:   」這 8 個字去掉 */
			/* 接下來從 /proc/[PID]/stat 取得 CPU 時間 */
			fscanf(statp, "%*d"); /* 讀掉 PID */
			while(getc(statp) != '('); /* 讀掉 command name 的左括弧 */
			for(i=1; i<= strlencount; i++){ /* 讀掉整個 command name */
				getc(statp);
			}
			while(getc(statp) != ')'); /* 讀掉 command name 的右括弧 */
			fscanf(statp, "%*s %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u"
					"%lu %lu %*d %*d %*d %*d %*d %*d %*u %lu", 
					&cpuuser, &cpusystem, &vmsize);
			syscpuall = 0;
			while(!feof(sysstatp)){
				fscanf(sysstatp, "%9s", hdrstore);
				if(!strcmp(hdrstore, "cpu")){
					while(fscanf(sysstatp, "%d", &cpuinfostore) == 1){
						syscpuall += cpuinfostore;
					}
					break;
				}else{
					while(getchar() != '\n');
				}
			}
			if(!firstrun){
				diffsyscpuall = syscpuall - presyscpuall;
				if(diffsyscpuall){
					res_cpuuser = 1000 * (cpuuser - precpuuser) / 
						diffsyscpuall;
					res_cpusystem = 1000 * (cpusystem - precpusystem) / 
						diffsyscpuall;
					printf("  user%%: %2d.%d  sys%%: %2d.%d",
						res_cpuuser / 10, res_cpuuser % 10,
						res_cpusystem / 10, res_cpusystem % 10);
				}else{
					fputs("  user%: ?     sys%: ?   ", stdout);
				}
			}else{
				fputs("  user%: ?     sys%: ?   ", stdout);
			}
			precpuuser = cpuuser;
			precpusystem = cpusystem;
			presyscpuall = syscpuall;
			printf("  VM: %lu KiB", vmsize / 1024);
			fputs("                    ", stdout);
		}
		if(statp != NULL){
			fclose(statp);
		}
		if(statusp != NULL){
			fclose(statusp);
		}
		if(sysstatp != NULL){
			fclose(sysstatp);
		}
		firstrun = 0;
#endif
		fflush(stdout);

		clock_gettime(CLOCK_REALTIME, &timecur);
		timeexpire.tv_sec = timecur.tv_sec + timesleep.tv_sec;
		timeexpire.tv_nsec = timecur.tv_nsec + timesleep.tv_nsec;
		checktimespec(&timeexpire);
	}while(sem_timedwait(&dispthr, &timeexpire));

	pthread_mutex_lock(&tdisplay_mx);
	tdisplay_yes = 0;
	pthread_mutex_unlock(&tdisplay_mx);

	return NULL;
}

