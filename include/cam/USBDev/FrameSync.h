#include "CommonTypes.h"

#ifndef __FRAME_SYNC
#define __FRAME_SYNC

//	Frame synchronizer.
//	Data structure:
//		FrameSync AuxData LineData ........
//		LineSync DummyData LineData ........
//		........
//		<< Next Frame >>

enum FRAME_READ_STATE : int
{
	WAIT_FRAME_SYNC = 0,
	WAIT_AUX_DATA,
	WAIT_AUX_HALF_DATA,
	WAIT_LINE_DATA,
	WAIT_LINE_SYNC,
	WAIT_DUMMY_DATA,
	WAIT_EOLF,
	WAIT_EOF_WIDTH,		// In a few cases the width information is in the next packet.
	WAIT_EOF_HEIGHT,		// In a few cases the width information is in the next packet.
};

class FrameSync
{
public:
	FrameSync();
	~FrameSync();

	BYTE * PutBuffer_ReducedSync(BYTE * pData, BOOL bAbort, int nLength, int * pWidth, int * pHeight);
	BYTE * PutBuffer_FastSync(BYTE * pData, BOOL bAbort, int nLength, int * pWidth, int * pHeight);

	BYTE * PutBuffer(BYTE * pData, BOOL bAbort, int nLength, int * pWidth, int * pHeight, UINT32 nFrameIndex = 0);

	void DropFrame();	// Drop bad frame. 

public:
	int		m_bWidthDoubler;

	UINT32	m_nVirtualChIndex;	//	Debug use only. Virtual channel index.

	//	Frame index control. Data index will always be updated per data packet and command frame index will be updated by cmd packet.
	//	If command header comes later, wait until command packet is received; else if command header comes earlier, wait for all data packets to come.
	//	Flush of old data if data packet header does not match.
	//	HW index always begin from 1. Software library index always begin from 0.
	UINT32	m_nCmdFrameIndex;		//	Frame index. Used to identify command packet header and data packet.
	UINT32	m_nDataFrameIndex;	//	Frame index. 

private:
	// Frame buffer is a 10MB buffer while data buffer is only by size of 1MB.
	// When a line is extracted the pointer will always be set to 0 for data buffer.
	BYTE * m_pFrameBuffer;		// Dual frame buffer for ping-pong operation
	UINT32 m_nFrameBufferSz;	//	Frame buffer size.

	int	 m_nPingPong;		// Current buffer
	int	 m_LineNo;			// Frame line number received 
	size_t m_LinePtr;			// Line pointer
	size_t m_FramePtr;		// Frame buffer pointer
	FRAME_READ_STATE m_State;

	// Width & Height are extracted from aux data.
	int		m_nWidth;
	int		m_nHeight;
public:
	int		m_nFrameTime;	//	Frame time (in cycles)
	int		m_nMilliSecTick;	//	Millisecond clock ticks (for frame rate calculation)

private:
	UINT64	m_nLinePix;
	UINT64	m_nLineIdx;

	// Synchronize words are Frame and Line Sync words. Only these two are used.
	UINT64	m_ulFrameSync;
	UINT64	m_ulLineSync;


public:
	int		m_nLineLost;
	int		m_iLastLine;

	void *	m_USBCtrl;

public:
	volatile UINT32	m_nParsedWidth;
	volatile UINT32	m_nParsedHeight;
	volatile UINT32	m_nParsedFrameIndex;

	volatile UINT32 SECOND_BUF_OFFSET;	

	bool		m_bAuxOffsetP1;

public:
	volatile UINT32	m_nTotalFrameLength;
	volatile UINT32	m_nTotalLeftBytes;

};

#endif