
#include <Windows.h>

#include "USBDev_Semaphore.h"

void* CreateLock()
{
	CRITICAL_SECTION* cs = new CRITICAL_SECTION();
	InitializeCriticalSection(cs);
	return cs;
}

void* CreateEventObject()
{
	HANDLE he = CreateEvent(NULL, FALSE, FALSE, NULL);
	return he;
}

void ActivateEvent(void* pevent)
{
	HANDLE he = (HANDLE)pevent;
	SetEvent(he);
}

UINT32 WaitForEvent(void* pevent, int timeout)
{
	HANDLE he = (HANDLE)pevent;
	return WaitForSingleObject(he, timeout);
}

UINT32 WAIT_TIMED_OUT = WAIT_TIMEOUT;

void DeleteSemaphore(void* plock)
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)plock;
	DeleteCriticalSection(cs);
	delete cs;
}

void DeleteEvent(void* pevent)
{
	HANDLE he = (HANDLE)pevent;
	CloseHandle(he);
}

UINT32 AcquireLock(void* plock, int timeout)
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)plock;
	EnterCriticalSection(cs);
	return 0;
}

UINT32 ReleaseLock(void* plock)
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)plock;
	LeaveCriticalSection(cs);
	return 0;
}
