#ifndef __USBDEV_SEMAPHORE
#define __USBDEV_SEMAPHORE

#include "../DEVDEF.h"

///////////////////////////////////////////////////////////////////////
//	General Semaphore Interface
///////////////////////////////////////////////////////////////////////

/* Create Semaphore
* Return non-zero if succeed.
*/
void* CreateLock();

/* Create Event
* Return non-zero if succeed.
*/
void* CreateEventObject();

/* Activate Event.
* Call this to set an event for consumer thread.
*/
void ActivateEvent(void* pevent);

/* Wait For Object
* Wait for something and return if timeout or happened.
* Return 0 if succeed; non-zero if timeout.
*/
UINT32 WaitForEvent(void* pevent, int timeout);

/* Delete Semaphore.
*/
void DeleteSemaphore(void* plock);

/* Delete Event.
*/
void DeleteEvent(void* pevent);

/* Lock Semaphore
* Return non-zero if failed.
*/
UINT32 AcquireLock(void* plock, int timeout);

/* Release Semaphore
* Return non-zero if failed.
*/
UINT32 ReleaseLock(void* plock);

extern UINT32 WAIT_TIMED_OUT;


#endif