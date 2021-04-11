#include "../Bayer2RGBA.h"

#include "../USBDev.h"
#include "../USBController.h"

///////////////////////////////////////////////////////////////////////////////////////////////
//	Virtual Channel
///////////////////////////////////////////////////////////////////////////////////////////////

VirtualChParam::VirtualChParam()
{
	m_nStreamIndex = 0;
	m_pImageFrame = NULL;
	m_pReceivedFrame = NULL;
	m_pLastImageFrame = NULL;
	m_nImageWidth = m_nImageHeight = 0;
	m_hThRecvImage = NULL;
	m_hThRecvUSBStream = NULL;
	m_hThRecvFrameEvent = CreateEventObject();
	m_bRecvUSBFail = false;
	m_cRecvUSBBuf = CreateLock();
	m_cFrameRcLock = CreateLock();
	//	Recording
	memset(m_sRecordFileName, 0, 4096);
	m_pRecordBufArray = NULL;
	m_nRecordBufMaxLen = 0;
	m_nRecordFrameCnt = 0;
	m_pEPStreamIn = NULL;
	m_nReceivedFrameBufSz = 0;
	m_bRecordRecvStream = false;
	m_nFreeSystemMem = 0;
	m_nDebugFrameWidth = 0;
	m_nDebugFrameHeight = 0;
	m_nVirtualChIndex = 2;
	m_Dev_Frame_Info.pFrameBuffer = NULL;
	m_Dev_Frame_Info.nFrameWidth = m_Dev_Frame_Info.nFrameHeight = m_Dev_Frame_Info.nTimeStamp = m_Dev_Frame_Info.nFrameCycle = m_Dev_Frame_Info.nCycleResolution = 0;
	m_pNextVirtualCh = NULL;
	m_cReceiveWaitEvent = CreateEventObject();
	m_bReceiveLocked = false;
	m_cRcvWaitDetLock = CreateLock();
}

VirtualChParam::~VirtualChParam()
{
	if(m_pNextVirtualCh)
	{
		delete m_pNextVirtualCh;
	}
	if(m_hThRecvUSBStream)
	{
		WaitForThread(m_hThRecvUSBStream, 0xFFFFFFFF);
		CloseThread(m_hThRecvUSBStream);
		m_hThRecvUSBStream = NULL;
	}
	if(m_hThRecvImage)
	{
		WaitForThread(m_hThRecvImage, 0xFFFFFFFF);
		CloseThread(m_hThRecvImage);
		m_hThRecvImage = NULL;
	}
	if(m_pReceivedFrame)
		delete []m_pReceivedFrame;
	m_nStreamIndex = 0;
	m_pImageFrame = NULL;
	m_pReceivedFrame = NULL;
	m_pLastImageFrame = NULL;
	m_nImageWidth = m_nImageHeight = 0;
	m_hThRecvImage = NULL;
	m_hThRecvUSBStream = NULL;
	m_bRecvUSBFail = false;
	//	Recording
	memset(m_sRecordFileName, 0, 4096);
	m_pRecordBufArray = NULL;
	m_nRecordBufMaxLen = 0;
	m_nRecordFrameCnt = 0;
	DeleteSemaphore(m_cRecvUSBBuf);
	DeleteSemaphore(m_cFrameRcLock);
	DeleteEvent(m_hThRecvFrameEvent);
	DeleteEvent(m_cReceiveWaitEvent);
	DeleteSemaphore(m_cRcvWaitDetLock);
}


///////////////////////////////////////////////////////////////////////////////////////////////
//	FrameRecvParam
///////////////////////////////////////////////////////////////////////////////////////////////

FrameRecvParam::FrameRecvParam()
{
	m_pFirstVirtualCh = new VirtualChParam();
	m_cRcvDetectLock = CreateLock();
	m_bReceiveActive = false;
	m_cCreateNewVChLock = CreateLock();
}
FrameRecvParam::~FrameRecvParam()
{
	delete m_pFirstVirtualCh;
	DeleteSemaphore(m_cRcvDetectLock);
	DeleteSemaphore(m_cCreateNewVChLock);
}

void FrameRecvParam::DropBadFrame()
{
	VirtualChParam* pVch = this->m_pFirstVirtualCh;
	while(pVch)
	{
		pVch->m_FrameSync.DropFrame();
		pVch = pVch->m_pNextVirtualCh;
	}
}

void FrameRecvParam::AddVirtualChannel(int virtch)
{
	if((virtch <= 0) || ((virtch & 0x0000FFFF) >= MAX_VIRTCH_CNT))
		return;
	AcquireLock(m_cCreateNewVChLock, 0xFFFFFFFF);
	//	First check for virtual channel.
	VirtualChParam* pVch = this->m_pFirstVirtualCh;
	VirtualChParam* pLastVch = NULL;
	while(pVch)
	{
		if(pVch->m_nVirtualChIndex == virtch)
			break;
		pLastVch = pVch;
		pVch = pVch->m_pNextVirtualCh;
	}
	//	If a virtual channel with the same index found, skip. Else create new at the end.
	if(pVch == NULL)
	{
		//	Not found.
		pLastVch->m_pNextVirtualCh = new VirtualChParam();
		pVch = pLastVch->m_pNextVirtualCh;
		pVch->m_nVirtualChIndex = virtch;
		pVch->m_pEPStreamIn = pLastVch->m_pEPStreamIn;
		pVch->m_FrameSync.m_nVirtualChIndex = virtch;
	}
	ReleaseLock(m_cCreateNewVChLock);
}

VirtualChParam* FrameRecvParam::GetVirtualChannel(int virtch)
{
	//	First check for virtual channel.
	VirtualChParam* pVch = this->m_pFirstVirtualCh;
	while(pVch)
	{
		if(pVch->m_nVirtualChIndex == virtch)
			return pVch;
		pVch = pVch->m_pNextVirtualCh;
	}
	return NULL;
}

bool FrameRecvParam::ObtainReceiveLock()
{
	bool bLockStatus = true;
	//	Lock RcvDetectLock first. All other threads try to obtain the lock will be blocked.
	AcquireLock(this->m_cRcvDetectLock, 0xFFFFFFFF);
	//	If the receive is active, the port is already obtained by another thread. Otherwise obtain.
	if(this->m_bReceiveActive)
		bLockStatus = false;
	else
		m_bReceiveActive = true;
	ReleaseLock(m_cRcvDetectLock);
	return bLockStatus;
}

void FrameRecvParam::ReleaseReceiveLock()
{
	m_bReceiveActive = false;

	//	Activate all threads. All threads will be waked up (if existed) to reduce complexity.
	VirtualChParam* pVch = this->m_pFirstVirtualCh;
	while(pVch)
	{
		ActivateEvent(pVch->m_cReceiveWaitEvent);
		pVch = pVch->m_pNextVirtualCh;
	}
}
