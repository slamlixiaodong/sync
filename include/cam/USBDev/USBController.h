#ifndef _USBCONTROLLER_H_
#define _USBCONTROLLER_H_

#include "CommonTypes.h"

#include "USBDev.h"

#include "DEVDEF.h"

#include "FrameSync.h"

#include <stdio.h>

#include <queue>

class USBController;

#include "Misc/USBDev_Threads.h"
#include "Misc/USBDev_Semaphore.h"

using namespace std;

#define MAX_PENDING_CMD 63

#define RECEIVE_DUMP_DATA FALSE

class VirtualChParam
{
public:
	void			*	m_pEPStreamIn;	// STREAM uses 0x81

	int				m_nStreamIndex;
	UCHAR			*	m_pImageFrame;

	UINT32			m_nVirtualChIndex;	//	Virtual channel index. Used in composite streams.

	UCHAR			*	m_pReceivedFrame;
	int				m_nReceivedFrameBufSz;
	UCHAR			*	m_pLastImageFrame;

	FrameSync			m_FrameSync;	// <<Require delete on exit>>

	bool				m_bRecvUSBFail;

	int				m_nImageWidth;
	int				m_nImageHeight;
	HANDLE			m_hThRecvImage;
	HANDLE			m_hThRecvUSBStream;
	HANDLE			m_hThRecvFrameEvent;

	queue<UCHAR*>		m_qRecvUSBBuf;		//	Queued buffer
	queue<BOOL>			m_qRecvAbort;		//	Queued buffer status. On receive failed, reset frame sync.

	void			*	m_cRecvUSBBuf;
	void			*	m_cFrameRcLock;

	char				m_sRecordFileName[4096];
	BYTE			**	m_pRecordBufArray;
	int				m_nRecordBufMaxLen;
	int				m_nRecordFrameCnt;

	UINT32			m_nFreeSystemMem;
	bool				m_bRecordRecvStream;

	int				m_nDebugFrameWidth;
	int				m_nDebugFrameHeight;

	DEV_FRAME_INFO		m_Dev_Frame_Info;		//	DEV_FRAME_INFO member for advanced frame acquisition.

public:
	VirtualChParam	*	m_pNextVirtualCh;

public:
	//	All threads that do not obtain receive lock must wait for this event for 1000 ms.
	void			*	m_cReceiveWaitEvent;
	void			*	m_cRcvWaitDetLock;
	bool				m_bReceiveLocked;

public:
	VirtualChParam();
	~VirtualChParam();
};

class FrameRecvParam
{
public:
	VirtualChParam	*	m_pFirstVirtualCh;

public:
	void				DropBadFrame();		//	Drop all bad frames

	void				AddVirtualChannel(int virtch);	//	Add a specific virtual channel

	VirtualChParam	*	GetVirtualChannel(int virtch);	//	Packed function to get a specific virtual channel by id.

	bool				ObtainReceiveLock();	//	Obtain receive lock. If succeed, return true. Otherwise return false.
	void				ReleaseReceiveLock();	//	Release receive lock. For all virtual channels check for locked threads and signal them. DO NOT CALL IF NOT LOCKED.

	FrameRecvParam();
	~FrameRecvParam();

private:

	//	In case of multiple virtual channel operation, only one thread is allowed to read from USB. 
	void			*	m_cRcvDetectLock;		//	Acquire this lock and then detect receive thread.
	bool				m_bReceiveActive;		//	Receive thread is active.

	void			*	m_cCreateNewVChLock;	//	Lock for new virtual channel creation.
};

class USBController
{
public:
	USBController();
	~USBController();

	//////////////////////////////////////////////////////////////////////////////
	//	Support Functions
	//////////////////////////////////////////////////////////////////////////////

	DEV_STATUS	OpenDevice(int device_index = 0, int init = 0);		//	Open device.
	DEV_STATUS	InitDevice();
	DEV_STATUS	CloseDevice();

	//	Firmware Control
	DEV_STATUS	GetDeviceID(UINT32* pid);										//	Get device ID
	DEV_STATUS	ValidateHardware(UINT32* pi);										//	Validate hardware using bit pattern (0x01, 0x03, 0x07, 0x0F, ....)

	int GetUSBDeviceCount();
	DEV_STATUS	DropRespInPackets();

	//	Endpoint operations
	DEV_STATUS	DEV_WriteCmdEndpoint(UINT32* pData, int timeout);						//	Write to command endpoint (1KB)
	DEV_STATUS	DEV_ReadRespEndpoint(UINT32* pData, int timeout);						//	Read from response endpoint (1KB)

	UCHAR * GetFrame(int streamindex, int virtualch, int * pWidth, int * pHeight, DEV_FRAME_INFO* pFrameInfo = NULL);

	//	Init sensors (separate, by hand)
	DEV_STATUS	LogSensorFailStatus(int nindex, int bfail, int breinit, UINT64 start);


	//////////////////////////////////////////////////////////////////////////////
	//	Device dependent operations
	//////////////////////////////////////////////////////////////////////////////
	DEV_STATUS	VerifyDeviceHWStatus();				//	Request bit-wise verification data


	//////////////////////////////////////////////////////////////////////////////
	//	Command & Response
	//////////////////////////////////////////////////////////////////////////////
	DEV_STATUS	CommitCommand(UINT32* pCmd, UINT32* pResp, bool bVerify, int timeout);		//	Send command and verify for response header (first 16 bytes)

	//	When a command packet is needed, call this interface to achieve easy access.
	DEV_STATUS	SendCommandPacket(int nIndex, int nCmd, UINT32 nData, UINT32 *pResp, bool bVerify, int timeout);


	//////////////////////////////////////////////////////////////////////////////
	//	IIC / SPI
	//////////////////////////////////////////////////////////////////////////////
	DEV_STATUS	IICGeneralAccess(BYTE slvaddr, BYTE *pRegAddr, BYTE *pRegData, UINT32 ralen, UINT32 dalen, UINT32 iicsel, UINT32 bR1W0);
	DEV_STATUS	WriteRegister(BYTE slvaddr, UINT32 nRegAddr, UINT32 nRegData, UINT32 ralen, UINT32 dalen, UINT32 iicsel);
	DEV_STATUS	ReadRegister(BYTE slvaddr, UINT32 nRegAddr, UINT32 ralen, UINT32 dalen, UINT32* rdto, UINT32 iicsel);


	//////////////////////////////////////////////////////////////////////////////
	//	PWM
	//////////////////////////////////////////////////////////////////////////////
	DEV_STATUS	SetPWMMode(UINT32 index, UINT32 period, UINT32 duty, UINT32 enable);

	//////////////////////////////////////////////////////////////////////////////
	//	Trigger / LED Functions
	//////////////////////////////////////////////////////////////////////////////
	DEV_STATUS	SetTriggerMode(UINT32 dwMode);
	DEV_STATUS	SetTriggerInterval(UINT32 dwInterval);
	DEV_STATUS	SetTriggerOnce(UINT32 dwTrig);
	DEV_STATUS	SetTriggerPinMode(UINT32 dwMode);
	DEV_STATUS	Set034ResetPin(UINT32 bReset);

	DEV_STATUS	Set034Initialize();

	DEV_STATUS	Set034LEDMode(UINT32 dwMode);
	DEV_STATUS	Set034LEDPolary(UINT32 dwPol);
	DEV_STATUS	Set034LEDOnTime(UINT32 dwTime);

	DEV_STATUS	Get034AECAGCStatus(UINT32* pData);			//	AEC/AGC data. Bit 0 AEC  Bit 1 AGC (0xAF)
	DEV_STATUS	Get034AECMinMaxCoarseInteg(UINT32* pData);	//	AEC min / max coarse integration time (0xAC, 0xAD)
	DEV_STATUS	Get034TotalInteg(UINT32* pData);			//	Total integration time (0x0B)
	DEV_STATUS	Get034KneePoints(UINT32* pData);			//	Integration knee points (0x08, 0x09)
	DEV_STATUS	Get034AGCMaxGain(UINT32* pData);			//	Max gain for AGC (0xAB)
	DEV_STATUS	Get034AnalogGain(UINT32* pData);			//	Current analog gain (0x35)
	DEV_STATUS	Get034DesiredBin(UINT32* pData);			//	Desired brightness (0xA5)
	DEV_STATUS	Get034HDRState(UINT32* pData);			//	HDR state (0x0A bit 8)
	DEV_STATUS	Get034KneeAutoAdjust(UINT32* pData);		//	Knee Auto Adjust

	DEV_STATUS	Set034AnalogGain(UINT32 bEnAGC, UINT32 gain, UINT32 maxgain);	//	Set analog gain
	DEV_STATUS	Set034Exposure(UINT32 bEnAEC, UINT32 bEnHDR, UINT32 bEnAutoAdjust, UINT32 coarseint, UINT32 knee1, UINT32 knee2, UINT32 minint, UINT32 maxint);	//	Set exposure
	DEV_STATUS	Set034DesiredBin(UINT32 nDesired);			//	Set desired bin

	DEV_STATUS	Set034Reset(UINT32 reset);				//	Set reset. 1: reset  0: do nothing

	DEV_STATUS	Set034BinMode(UINT32 bin);				//	Set bin mode. 0: 1X  1: 2X  2: 4X  Others: 0

public:

	UINT32			m_b034AGCEnable;
	UINT32			m_b034AECEnable;
	UINT32			m_n034BinMode;

public:
	BOOL				m_bCloseDevice;
	BOOL				m_bDeviceOpened;

	UINT32			m_nDeviceIndex;

	UINT32			m_nCmdRespPacketSize;		//	Device type. USB2 use 512B, and USB3 use 1024B.

	UINT32			m_nSensorStatus;			//	Sensor status. Success if OK, initfailed if Aborted.

	//	Device flash address map
	UINT32			m_nSPI_FlashSize;			//	SPI flash size. Default to 16MB.
	UINT32			m_nSPI_FPGABaseAddr;
	UINT32			m_nSPI_FWBaseAddr;
	UINT32			m_nSPI_ConfigBaseAddr;
	UINT32			m_nSPI_SignatureBaseAddr;

	//	Flash block / sector identification
	UINT32			m_nSPI_JEDECID;
	UINT32			m_nSPI_BlockSize;
	BOOL				m_bSPI_Support4K;
	UINT32			m_nSPI_DevIndex;

	UINT32			m_nMagicDataBuf[16384];			//	Buffered version of MagicData. Export for USBDev OpenDevice only. 64 KB in size.

public:
	void			*	m_pUSBDevice;	// <<Require delete on exit>>

	void			*	m_pEPCmdOut;	//	CMD uses 0x01
	void			*	m_pEPRespIn;	//	RESP uses 0x82
	void			*	m_pEPStreamIn;	//	STREAM uses 0x81

	void			*	m_thEPBulkOut;	//	If used, stores the bulk out thread.

public:

	int				m_nWidthDivide;	// Width divide factor. Used when V5000A in global shutter mode.
	int				m_nHeightMultiply;// Height multiply factor. Used when V5000A in global shutter mode.

//public:
//	void			*	m_pStreamLock;	// Global lock
//	void			*	m_pCmdLock;		// Global lock

public:
	bool				m_bDropBadFrame;


	double			m_dTransferDataRate;
	double			m_dTransferFrameRate;

public:
	//FILE			*	m_pLogFile;
	//void			*	m_pLogFileLock;
	//void				_WriteLogString(char* pstr);
	//UINT64			m_LogTmStart;
	//bool				_Debug_Log;

public:
	//	Fast crop on serial sensors
	int				m_nCropBaseXOrg;
	int				m_nCropBaseYOrg;
	bool				m_bCropBaseOrgEn;

	//	Crop origin output (for external algorithm)
	int				m_nCropBaseXOrg_Total[16];
	int				m_nCropBaseYOrg_Total[16];
	int				m_nCropBaseWidth_Total[16];
	int				m_nCropBaseHeight_Total[16];

public:
	//	Multiple Stream
	//	If a camera contains multiple streams, each stream should be registered separately.
	//	The multiple stream operation supports single thread only.
	//	The number of streams should be represented by some way.
	int				m_nStreamCount;
	FrameRecvParam	*	m_pFrameRecvParam;

	//	Stream lost detection
	int				m_nStreamLostConfirm[MAX_STREAM_CNT * MAX_VIRTCH_CNT];	//	Lost detection confirmation. If confirmed, log time.
	UINT64			m_nStreamLostBegin[MAX_STREAM_CNT * MAX_VIRTCH_CNT];		//	Lost detection begin time. Wait 2 seconds for recovery.

	//	MultiThreaded
	bool				m_bMultiThreaded;

public:
	//	File handle for recording raw data (from capture module)
	FILE			*	m_pRawRecordFile;
	BOOL				m_bRecordThreadSt;

public:
	bool				m_bPauseRecvStream;

	int				m_nSensorFrameCyc;

public:
	// NSC1105 configuration words
	UINT64			m_uNSC1105_Config;


	//	Debug Signals
public:
	bool				m_bDebugBreak;

	//	Thread Active Signals
public:
	volatile bool		m_bActive_ReceiveUSBStreamAndRecord;
};

#endif