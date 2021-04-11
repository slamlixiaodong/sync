#include "../Bayer2RGBA.h"

#include "../USBDev.h"
#include "../USBController.h"
#include "../USBIO/USBIO.h"

extern volatile int gbPrintDebugInfo;

__inline BYTE* GetLine(BYTE* pbuffer, int lineno, int width, int height, bool bRGB)
{
	if(lineno == -1)
		return pbuffer + width * (bRGB ? 4 : 1);
	else if(lineno == height)
		return pbuffer + (lineno - 2) * width * (bRGB ? 4 : 1);
	else if(lineno >= 0 && lineno < height)
		return pbuffer + lineno * width * (bRGB ? 4 : 1);
	else
		return NULL;
}


UCHAR * USBController::GetFrame(int streamindex, int virtualch, int * pWidth, int * pHeight, DEV_FRAME_INFO* pFrameInfo)
{
	//	Clear at first
	if(pFrameInfo)
		memset(pFrameInfo, 0, sizeof(DEV_FRAME_INFO));
	*pWidth = 0;
	*pHeight = 0;

	if(!m_pUSBDevice)
		return NULL;
	if(streamindex >= m_nStreamCount)
		return NULL;
	FrameRecvParam* pParam = &m_pFrameRecvParam[streamindex];
	if(pParam->m_pFirstVirtualCh->m_pEPStreamIn == NULL)
		return NULL;
	//	Only advanced_sync supports multiple virtual channels.
	if(virtualch != 2)
		return NULL;
	//	Get the virtual channel controller. If not existed, create one if within the limit.
	VirtualChParam* pVch = pParam->GetVirtualChannel(virtualch);
	if(pVch == NULL)
	{
		if(gbPrintDebugInfo >= LOG_CRITICAL)
		{
			char pLogBuf[1024];
			sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: Creating new virtual channel parser for %08x.", __LINE__, virtualch);
			printf("%s\r\n", pLogBuf);
			DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
		}
		if((virtualch & 0xFFFF) < MAX_VIRTCH_CNT)
			pParam->AddVirtualChannel(virtualch);
		pVch = pParam->GetVirtualChannel(virtualch);
		if(pVch == NULL)
			return NULL;
	}
	int lastvchindex = pVch->m_nVirtualChIndex;

	UCHAR * pRcvBuf = NULL;

	m_bMultiThreaded = false;
	if(m_bMultiThreaded == false)
	{
		if(gbPrintDebugInfo >= LOG_ALL)
		{
			char pLogBuf[1024];
			sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: Begin transfer stream %d.", __LINE__, streamindex);
			printf("%s\r\n", pLogBuf);
			DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
		}
		BYTE pBuf[SZSTREAM_BUFFER];
		int nRetried = 0;
		UINT64 nStartMilliSec = DEV_GetCurrentMilliSecond();
		//	Pre chear frame buffer pointer
		pVch->m_Dev_Frame_Info.pFrameBuffer = NULL;

		//	If we want to receive, try to lock the buffer first.
		bool bReceiveLockObtained = false;
		while(1)
		{
			bReceiveLockObtained = pParam->ObtainReceiveLock();
			if(bReceiveLockObtained == false)
			{
				UINT32 waitret = WaitForEvent(pVch->m_cReceiveWaitEvent, 500);
				//	Check for wait time. No more than 1000 can be wait.
				UINT64 nEndMilliSec = DEV_GetCurrentMilliSecond();
				if(pVch->m_Dev_Frame_Info.pFrameBuffer)
				{
					//	If frame is received by another thread, return.
					*pWidth = pVch->m_Dev_Frame_Info.nFrameWidth;
					*pHeight = pVch->m_Dev_Frame_Info.nFrameHeight;
					if(pFrameInfo)
						*pFrameInfo = pVch->m_Dev_Frame_Info;
					break;
				}
				if((nEndMilliSec - nStartMilliSec) > 1000)
				{
					//	If time out, we may consider Hang state.
					pParam->ReleaseReceiveLock();
					return NULL;
				}
				if(m_bCloseDevice)
					break;
			}
			else
			{
				//	ReceiveLock obtained. Begin transfer.
				//	At any time if frame of another virtual channel parsed, wake the thread.
				while(1)
				{
					int bufLen = SZSTREAM_BUFFER;
					bool bRet = TransferInEndpoint(pVch->m_pEPStreamIn, pBuf, &bufLen, 1000);
					if(!bRet)
					{
						// Reset pipe and try again
						ResetEndpoint(pVch->m_pEPStreamIn);
						bufLen = SZSTREAM_BUFFER;
						if(m_bDropBadFrame)
							pParam->DropBadFrame();
						bRet = TransferInEndpoint(pVch->m_pEPStreamIn, pBuf, &bufLen, 1000);
					}
					if(bRet)
					{
						//	Parse data. For Advanced Sync mode, packets should be splitted to virtual channel and command packet.
						//	For test we only implement standard single virtual channel.
						//	If the current stream is not advanced, receive as its own.
						pRcvBuf = pVch->m_FrameSync.PutBuffer(pBuf, FALSE, bufLen, pWidth, pHeight);
						if(pRcvBuf)
						{
							pVch->m_Dev_Frame_Info.pFrameBuffer = pRcvBuf;
							pVch->m_Dev_Frame_Info.nFrameCycle = pVch->m_FrameSync.m_nFrameTime;
							pVch->m_Dev_Frame_Info.nCycleResolution = pVch->m_FrameSync.m_nMilliSecTick;
						}
					}
					else
					{
						pVch->m_Dev_Frame_Info.pFrameBuffer = NULL;
						if(gbPrintDebugInfo >= LOG_CRITICAL)
						{
							char pLogBuf[1024];
							sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: No data received from stream channel.", __LINE__);
							printf("%s\r\n", pLogBuf);
							DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
						}
						pParam->ReleaseReceiveLock();
						bReceiveLockObtained = false;
						return NULL;
					}
					//	If the frame is received, break.
					if(pVch->m_Dev_Frame_Info.pFrameBuffer || m_bCloseDevice)
					{
						pParam->ReleaseReceiveLock();
						bReceiveLockObtained = false;
						break;
					}
					nRetried++;
					//	To determine force block of stream read, receive for at most 128MB before frame valid.
					if(nRetried > (128 * 1024 * 1024 / SZSTREAM_BUFFER))
					{
						if(gbPrintDebugInfo >= LOG_CRITICAL)
						{
							char pLogBuf[1024];
							sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: End transfer stream %d %s.", __LINE__, streamindex, "failed. Transfer out of size");
							printf("%s\r\n", pLogBuf);
							DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
						}
						pParam->ReleaseReceiveLock();
						bReceiveLockObtained = false;
						return NULL;
					}
					UINT64 nEndMilliSec = DEV_GetCurrentMilliSecond();
					if((nEndMilliSec - nStartMilliSec) > 1000)
					{
						pParam->ReleaseReceiveLock();
						bReceiveLockObtained = false;
						return NULL;
					}
				}
				if(pVch->m_Dev_Frame_Info.pFrameBuffer)
					break;
			}
			if(m_bCloseDevice)
				break;
		}

		//	On complete, if the receive lock is not returned in case, release.
		if(bReceiveLockObtained)
		{
			pParam->ReleaseReceiveLock();
			bReceiveLockObtained = false;
		}

		if(m_bCloseDevice)
		{
			if(gbPrintDebugInfo >= LOG_CRITICAL)
			{
				char pLogBuf[1024];
				sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: End transfer stream %d %s.", __LINE__, streamindex, "failed. Program terminating");
				printf("%s\r\n", pLogBuf);
				DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
			}
			return NULL;
		}

		//	Process frame data
		if(pVch->m_Dev_Frame_Info.pFrameBuffer)
		{
			//	Set DEV_FRAME_INFO with appropriate data.
			pRcvBuf = pVch->m_Dev_Frame_Info.pFrameBuffer;
			pVch->m_Dev_Frame_Info.nFrameWidth = *pWidth;
			pVch->m_Dev_Frame_Info.nFrameHeight = *pHeight;
			pVch->m_Dev_Frame_Info.nTimeStamp = 0;
			if(pFrameInfo)
				*pFrameInfo = pVch->m_Dev_Frame_Info;

			//	Post processing
			if(*pWidth != pVch->m_nDebugFrameWidth || *pHeight != pVch->m_nDebugFrameHeight)
			{
				if(gbPrintDebugInfo >= LOG_CRITICAL)
				{
					char pMsg[256];
					sprintf(pMsg, "File USBDevice_GetFrame.cpp Line %5d: Stream %d Frame info: %d %d", __LINE__, streamindex, *pWidth, *pHeight);
					printf("%s\r\n", pMsg);
					DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pMsg);
				}
				pVch->m_nDebugFrameWidth = *pWidth;
				pVch->m_nDebugFrameHeight = *pHeight;
			}
			UINT64 nEndMilliSec = DEV_GetCurrentMilliSecond();
			if(gbPrintDebugInfo >= LOG_UNIMPORTANT)
			{
				char pLogBuf[1024];
				sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: Stream %d get frame time interval: %d ms", __LINE__, streamindex, (int)(nEndMilliSec - nStartMilliSec));
				printf("%s\r\n", pLogBuf);
				DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
			}
			if(gbPrintDebugInfo >= LOG_ALL)
			{
				char pLogBuf[1024];
				sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: End transfer stream %d %s.", __LINE__, streamindex, pRcvBuf ? "success" : "fail");
				printf("%s\r\n", pLogBuf);
				DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
			}
			if(gbPrintDebugInfo >= LOG_UNIMPORTANT)
			{
				char pLogBuf[1024];
				sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: Frame ID: %d Stream Type: %s  Width: %d  Height: %d  Timestamp: %d Framerate: %f", __LINE__, pVch->m_FrameSync.m_nParsedFrameIndex, "Reduced", pVch->m_Dev_Frame_Info.nFrameWidth, pVch->m_Dev_Frame_Info.nFrameHeight, pVch->m_Dev_Frame_Info.nTimeStamp, 1000000000.0 / (double)pVch->m_Dev_Frame_Info.nFrameCycle / (1000000.0 / (double)pVch->m_Dev_Frame_Info.nCycleResolution));
				printf("%s\r\n", pLogBuf);
				DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
			}
			return pRcvBuf;
		}
		if(gbPrintDebugInfo >= LOG_CRITICAL)
		{
			char pLogBuf[1024];
			sprintf(pLogBuf, "File USBDevice_GetFrame.cpp Line %5d: End transfer stream %d %s.", __LINE__, streamindex, "fail");
			printf("%s\r\n", pLogBuf);
			DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
		}
		return NULL;
	}
	return NULL;
}
