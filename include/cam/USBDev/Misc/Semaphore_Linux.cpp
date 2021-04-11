
#include "USBDev_Semaphore.h"

#include <pthread.h>

#include <time.h>
#include <sys/time.h>

#include <errno.h>

typedef struct {
	pthread_cond_t* cond;
	pthread_mutex_t* mutex;
} Event_Pair;

void* CreateLock()
{
	//	Create a mutex and init. If fail, return NULL.
	pthread_mutex_t* mutex = new pthread_mutex_t();
	int err = pthread_mutex_init(mutex, NULL);
	bool success = true;
	if(err != 0)
	{
		delete mutex;
		mutex = NULL;
		success = false;
	}
	return mutex;
}

void* CreateEventObject()
{
	Event_Pair* pair = new Event_Pair();
	pair->cond = new pthread_cond_t();
	pair->mutex = new pthread_mutex_t();
	//	Init cond. If failed, clear cond.
	int err = pthread_cond_init(pair->cond, NULL);
	bool success = true;
	if(err != 0)
	{
		delete pair->cond;
		delete pair->mutex;
		delete pair;
		pair = NULL;
		success = false;
	}
	else
	{
		err = pthread_mutex_init(pair->mutex, NULL);
		success = true;
		if(err != 0)
		{
			pthread_cond_destroy(pair->cond);
			delete pair->cond;
			delete pair->mutex;
			delete pair;
			pair = NULL;
			success = false;
		}
	}
	return pair;
}

void ActivateEvent(void* pevent)
{
	Event_Pair* pair = (Event_Pair*)pevent;
	pthread_cond_signal(pair->cond);
}

UINT32 WaitForEvent(void* pevent, int timeout)
{
	Event_Pair* pair = (Event_Pair*)pevent;
	timespec to;
	timeval now;
	gettimeofday(&now, NULL);

	AcquireLock(pair->mutex, 0xFFFFFFFF);
	//	The input time is ms.
	to.tv_sec = time(NULL) + (timeout / 1000);
	UINT64 u_nsec = now.tv_usec * 1000 + (timeout % 1000) * 1000000;
	to.tv_nsec = u_nsec % 1000000000;
	to.tv_sec += u_nsec / 1000000000;
	int err = pthread_cond_timedwait(pair->cond, pair->mutex, &to);
	ReleaseLock(pair->mutex);
	return err;
}

UINT32 WAIT_TIMED_OUT = ETIMEDOUT;

void DeleteSemaphore(void* plock)
{
	pthread_mutex_t* mutex = (pthread_mutex_t*)plock;
	pthread_mutex_destroy(mutex);
	delete mutex;
}

void DeleteEvent(void* pevent)
{
	Event_Pair* pair = (Event_Pair*)pevent;
	DeleteSemaphore(pair->mutex);
	pthread_cond_destroy(pair->cond);
	delete pair->cond;
	delete pair;
}

UINT32 AcquireLock(void* plock, int timeout)
{
	pthread_mutex_t* mutex = (pthread_mutex_t*)plock;
	return pthread_mutex_lock(mutex);
}

UINT32 ReleaseLock(void* plock)
{
	pthread_mutex_t* mutex = (pthread_mutex_t*)plock;
	return pthread_mutex_unlock(mutex);
}