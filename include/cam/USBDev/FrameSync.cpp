#include "FrameSync.h"
#include "DEVDEF.h"
#include "USBController.h"

#include <iostream>
#include <string.h>

#define MIN_FRAME_HEIGHT 32

FrameSync::FrameSync()
{
	SECOND_BUF_OFFSET = DEV_MAX_FRAME_WIDTH * DEV_MAX_FRAME_HEIGHT;
	m_pFrameBuffer = new BYTE[SECOND_BUF_OFFSET * 2];
	m_nFrameBufferSz = SECOND_BUF_OFFSET * 2;
	m_nWidth = 0;
	m_nHeight = 0;
	m_nLinePix = 0;
	m_nLineIdx = 0;
	m_State = WAIT_FRAME_SYNC;
	m_LineNo = 0;
	m_LinePtr = 0;
	m_FramePtr = 0;
	m_nPingPong = 0;
	m_nLineLost = 0;
	m_iLastLine = 0;
	m_USBCtrl = NULL;
	m_bAuxOffsetP1 = false;
	m_nVirtualChIndex = 2;
	m_nCmdFrameIndex = 0;
	m_nDataFrameIndex = 0;

	m_nParsedWidth = 0;
	m_nParsedHeight = 0;
	m_nTotalFrameLength = 0;
	m_nTotalLeftBytes = 0;
	m_nParsedFrameIndex = 0;

	m_nFrameTime = 0;
	m_nMilliSecTick = 0;
}

FrameSync::~FrameSync()
{
	delete []m_pFrameBuffer;
}

void FrameSync::DropFrame()
{
	// Just clear frame buffer if error occurs.
	m_State = WAIT_FRAME_SYNC;

	m_nWidth = 0;
	m_nHeight = 0;

	m_nLinePix = 0;
	m_nLineIdx = 0;

	m_LineNo = 0;
	m_LinePtr = 0;
	m_FramePtr = 0;
	m_nLineLost = 0;
	m_iLastLine = 0;
}

BYTE * FrameSync::PutBuffer(BYTE * pData, BOOL bAbort, int nLength, int * pWidth, int * pHeight, UINT32 nFrameIndex)
{
		return PutBuffer_FastSync(pData, bAbort, nLength, pWidth, pHeight);
}

BYTE * FrameSync::PutBuffer_FastSync(BYTE * pData, BOOL bAbort, int nLength, int * pWidth, int * pHeight)
{

	//	UINT32 buffer assignment
	UINT32 uLLength = (nLength >> 2);
	UINT32* puData = (UINT32*)pData;

	// Check flag
	//	1MB is at most one frame for 720P image. However in case of even smaller image size we should drop extra data.
	size_t i = 0, j = 0;

	BYTE * pExtractedBuffer = NULL;

	BOOL bCopyFrame = FALSE;

	//	Abort frame on bAbort.
	if(bAbort)
	{
		m_State = WAIT_FRAME_SYNC;
	}

FASTSYNC_PARSE_AGAIN:

	BYTE * pFrameBuffer = m_pFrameBuffer + (m_nPingPong ? SECOND_BUF_OFFSET : 0);
	if(bCopyFrame)
	{
		//if(m_nParsedWidth == 640 * 240)
		//{
		//	*pWidth = m_nParsedWidth / 240;
		//	*pHeight = m_nParsedHeight * 240;
		//}
		//else
		{
			*pWidth = m_nParsedWidth;
			*pHeight = m_nParsedHeight;
			//memcpy(m_pOutFrame, pExtractedBuffer, m_nParsedWidth * m_nParsedHeight);
		}
		//memcpy(pFrameBuffer, pLastFrameBuffer, (*pWidth) * (*pHeight));
	}
	bCopyFrame = FALSE;

	// If pointer exceeds maximum return.
	if(i >= uLLength)
		return pExtractedBuffer;

	BYTE * pFrameHdr = 0;
	BYTE * pAuxData = 0;
	BYTE * pLineHdr = 0;
	BYTE * pLineData = 0;

	BOOL bIsFrameEnd = FALSE;

	UINT32 sync1 = 0;
	UINT32 sync2 = 0;

	UINT32 uFrameInfoLineCnt = 0;

	switch(m_State)
	{
	case WAIT_FRAME_SYNC:
		sync1 = STREAM_FAST_SYNC;
		m_nTotalFrameLength = 0;
		m_nTotalLeftBytes = 0;
		m_FramePtr = 0;
		//i = i - (i % 128);
		//	The frame header always exists at 512B boundary. 
		//	Always searches 512B boundary to look for header.
		for(; i < uLLength; i++)
		{
			if(puData[i] == (UINT32)sync1)
				break;
		}
		//	Out of range. Return.
		if(i >= uLLength)
			return pExtractedBuffer;
		i++;
		pAuxData = pData + (i << 2);
		i += 3;
		pLineData = pData + (i << 2);
		break;

	case WAIT_LINE_DATA:
		pLineData = pData + (i << 2);
		break;

	default:
		//	Do nothing here
		break;
	}

	//	Extract line index and pixel count per line.
	if(pAuxData)
	{
		m_nParsedWidth = (((UINT32*)pAuxData)[0]) & 0xFFFF;
		m_nParsedHeight = ((((UINT32*)pAuxData)[0] >> 16) & 0xFFFF);
		m_nFrameTime = (((UINT32*)pAuxData)[1]);
		m_nMilliSecTick = (((UINT32*)pAuxData)[2]);
		//if(m_nParsedWidth != 480 || m_nParsedHeight != 4320)
		//{
		//	((USBController*)m_USBCtrl)->_WriteLogString("Frame width or height does not match.");
		//}
		m_nParsedWidth *= 4;
		//	Currently we do not have padding information contained. So set to 4 next.
		m_nTotalFrameLength = (m_nParsedWidth * m_nParsedHeight);
		if((m_nParsedHeight * m_nParsedWidth) > (DEV_MAX_FRAME_WIDTH * DEV_MAX_FRAME_HEIGHT))
		{
			//	On receive failed, abort current frame.
			//	Due to the fact a frame can never be smaller than 1KB, abort current packet.
			m_State = WAIT_FRAME_SYNC;
			return pExtractedBuffer;
			// goto FASTSYNC_PARSE_AGAIN;
		}

		m_nTotalLeftBytes = m_nTotalFrameLength;
		m_iLastLine = m_nLineIdx;
	}

	if(pLineData)
	{
		// Copy line data.
		size_t nBytesLeft = nLength - (pLineData - pData);
		// If more bytes left then copy and recheck. Else copy a portion.
		if(nBytesLeft >= m_nTotalLeftBytes)
		{
			// 1. Copy buffer line data.
			// 2. Increment buffer pointer.
			// 3. Increment line number.
			// 4. If a full frame is received, set state to wait_frame_sync. Else set to wait_line_sync.
			// 5. Continue if not full. Always check for output buffer.
			memcpy(pFrameBuffer + m_FramePtr, pLineData, m_nTotalLeftBytes);
			i += (m_nTotalLeftBytes >> 2);
			m_FramePtr += m_nTotalLeftBytes;
			m_nTotalLeftBytes = 0;

			m_State = WAIT_FRAME_SYNC;
			pExtractedBuffer = pFrameBuffer;
			m_nPingPong = 1 - m_nPingPong;	// Switch ping-pong
			m_FramePtr = 0;
			bCopyFrame = TRUE;

			// Parse again for left data.
			goto FASTSYNC_PARSE_AGAIN;
		}
		else
		{
			// Copy a portion and return.
			memcpy(pFrameBuffer + m_FramePtr, pLineData, nBytesLeft);
			m_FramePtr += nBytesLeft;
			m_LinePtr += nBytesLeft;
			m_nTotalLeftBytes -= nBytesLeft;
			m_State = WAIT_LINE_DATA;
			return pExtractedBuffer;
		}
	}

	// Usually this means the buffer is empty. Just return.
	return pExtractedBuffer;
}

