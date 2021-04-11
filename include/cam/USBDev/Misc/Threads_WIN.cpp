#include <Windows.h>
#include <sys/timeb.h>
#include <time.h>

#include "USBDev_Threads.h"

void ThreadSleep(int millisec)
{
	Sleep(millisec);
}

UINT64 DEV_GetCurrentMilliSecond()
{
	__timeb64 tm64;
	_ftime64(&tm64);
	SYSTEMTIME systm;
	GetLocalTime(&systm);
	UINT64 millitm = tm64.time * 1000 + systm.wMilliseconds;
	return millitm;
}

USBDev_Time DEV_GetCurrentTime()
{
	USBDev_Time newtm;
	__timeb64 tm64;
	_ftime64(&tm64);
	SYSTEMTIME systm;
	GetLocalTime(&systm);
	newtm.year = systm.wYear;
	newtm.month = systm.wMonth;
	newtm.day = systm.wDay;
	newtm.hour = systm.wHour;
	newtm.minute = systm.wMinute;
	newtm.second = systm.wSecond;
	newtm.millisecond = systm.wMilliseconds;
	return newtm;
}

void* CreateThread(void* func, void* param)
{
	return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, param, 0, NULL);
}

void CloseThread(void* pthread)
{
	CloseHandle(pthread);
}

UINT32 WaitForThread(void* pthread, int timeout)
{
	return WaitForSingleObject(pthread, timeout);
}

