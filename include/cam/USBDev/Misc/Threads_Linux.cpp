
#include "USBDev_Threads.h"

#include <pthread.h>
#include <unistd.h>

#include <time.h>
#include <sys/time.h>

void ThreadSleep(int millisec)
{
	usleep(millisec << 10);
}

UINT64 DEV_GetCurrentMilliSecond()
{
	timeval now;
	gettimeofday(&now, NULL);
	UINT64 umillisec = time(NULL);
	umillisec *= 1000;
	umillisec += now.tv_usec / 1000;
	return umillisec;
}

USBDev_Time DEV_GetCurrentTime()
{
	timespec time;
	timeval val;
	clock_gettime(CLOCK_REALTIME, &time);
	gettimeofday(&val, NULL);
	tm now;
	localtime_r(&time.tv_sec, &now);
	USBDev_Time tm;
	tm.year = now.tm_year + 1900;
	tm.month = now.tm_mon + 1;
	tm.day = now.tm_mday;
	tm.hour = now.tm_hour;
	tm.minute = now.tm_min;
	tm.second = now.tm_sec;
	tm.millisecond = val.tv_usec / 1000;
	return tm;
}

void* CreateThread(void* func, void* param)
{
	//	Create a thread. If failed, return NULL.
	pthread_t* th = new pthread_t();
	int err = pthread_create(th, NULL, (void*(*)(void*))func, param);
	bool success = true;
	if(err != 0)
		success = false;
	if(success)
		return th;
	delete th;
	return NULL;
}

void CloseThread(void* pthread)
{
	//	Join in WaitForThread. Delete in CloseThread. Do not forget to call WaitForThread.
	pthread_t* th = (pthread_t*)pthread;
	//pthread_join(*th, NULL);
	delete th;
}

UINT32 WaitForThread(void* pthread, int timeout)
{
	//	Currently for Linux we do not deal with timed join.
	pthread_t* th = (pthread_t*)pthread;
	pthread_join(*th, NULL);
	return 0;
}

