
#include "USBDev.h"

#include "Misc/USBDev_Semaphore.h"
#include "Misc/USBDev_Threads.h"

#include "USBController.h"

#include "USBIO/USBIO.h"

#include "Bayer2RGBA.h"

USBController * gUsbController[MAX_DEV_CNT] = {NULL};
void * gUsbCmdLock[MAX_DEV_CNT] = {NULL};
void *** gUsbStreamLock[MAX_DEV_CNT] = {NULL};

static BOOL g_bDeviceClosed = true;				//	Indicate device is closed.
static BOOL g_bPutDeviceSlot = true;			//	Indicate device to be put into slot (after open). Set if in engineering mode.
static BOOL g_bInitForCapturing = false;			//	Indicate device to be initialized even if not into slot. Set if in capturing mode.
static BOOL g_bOpenDeviceOnly = false;			//	Open device only. No additional initialization sequence. 
static BOOL g_bUseHardwareMonitor = USEHWMONITOR;	//	Hardware monitor thread. Default off.

volatile int gbPrintDebugInfo = 0;

volatile int SENSOR_REINIT_PERIOD	= 3000;		//	Wait 3000 ms before reset a sensor (when marked fail)

int DEV_MAX_FRAME_WIDTH = DEFAULT_MAX_FRAME_WIDTH;
int DEV_MAX_FRAME_HEIGHT = DEFAULT_MAX_FRAME_HEIGHT;

volatile int TOF_TRIGGER_DELAY_TIME = 5;				//	Default TOF trigger delay time (10 ms)
volatile int TOF_DECODER_SF_OVERRIDE = 0;				//	TOF sub frame override. For reduced timing, change to small factor calculation. 0 cancel.

//	Global log file
FILE* pLogFile = NULL;
int nLogFileIndex = 0;					//	Log file index. 0 for local and 1 for tmp.
void* m_pLogFileLock = NULL;
UINT64 m_LogTmStart = 0;

//	Hardware monitor (single device only)
void* g_thHardwareMonitor = NULL;
volatile bool g_bHardwareMonitor = false;
const int g_nMaxRetryCount = 2;			//	Define the maximum retry times for device online detection.
volatile bool g_bBypassAllCmd = false;		//	Bypass of all commands

volatile bool g_bTOFDataIsEffective = true;	//	TOF data is effective. Currently valid in module only.

volatile int g_nIrisFrameWidth[16];		//	Iris frame width
volatile int g_nIrisFrameHeight[16];		//	Iris frame height
volatile int g_nIrisFrameCount[2] = {0, 0};	//	Iris frame count (per camera). Clear at any opendevice or reset.

#define	MAX_LOGFILE_SZ	(50 * 1024 * 1024)	//	Limit log file size to 50MB.

DEV_STATUS OpenDevice();

int DEV_QueryDevice()
{
	if(gbPrintDebugInfo >= LOG_CRITICAL)
	{
		char pLog[4096];
		sprintf(pLog, "File USBDev.cpp Line %5d: Begin query device count\r\n", __LINE__);
		printf(pLog);
		DEV_WriteLogFile(0, 0, (BYTE*)pLog);
	}
	USBController* pController = new USBController();
	int nDevCnt = pController->GetUSBDeviceCount();
	if(gbPrintDebugInfo >= LOG_CRITICAL)
	{
		char pLog[4096];
		sprintf(pLog, "File USBDev.cpp Line %5d: End query device count. %d device(s) found.\r\n", __LINE__, nDevCnt);
		printf(pLog);
		DEV_WriteLogFile(0, 0, (BYTE*)pLog);
	}
	delete pController;
	//printf("Device query finished successfully.\r\n");
	return nDevCnt;
}

UINT32 DEV_GetDeviceList()
{
	UINT32 uList = 0;
	for(int i = 0; i < MAX_DEV_CNT; i++)
	{
		if(gUsbController[i])
			uList |= (1 << i);
	}
	return uList;
}

void InitSignals()
{
	UINT32 ua, ub;
	ua = (UINT32)(int)-6645;
	ub = 23;
	UINT32 uc = (ua / ub) - (UINT32)(0xFFFFFFFF / ub) - (UINT32)(1 / ub);
	UINT32 ud = (ua % ub) - (UINT32)(0x0000000100000000 % (UINT64)ub);

	uc = (UINT32)((int)ua / (int)ub);
	ud = (UINT32)((int)ua % (int)ub);

	for(int i = 0; i < MAX_DEV_CNT; i++)
	{
		// Init critical section
		if(gUsbCmdLock[i] == NULL)
		{
			gUsbCmdLock[i] = CreateLock();
		}
		if(gUsbStreamLock[i] == NULL)
		{
			gUsbStreamLock[i] = new void**[MAX_STREAM_CNT];
			for(int j = 0; j < MAX_STREAM_CNT; j++)
			{
				gUsbStreamLock[i][j] = new void*[MAX_VIRTCH_CNT];
				for(int k = 0; k < MAX_VIRTCH_CNT; k++)
				{
					gUsbStreamLock[i][j][k] = CreateLock();
				}
			}
		}
		gUsbController[i] = NULL;
	}
	//	Try open log file at "./USBDev.log" first. Then open "/tmp/USBDev.log". 
	//	If the last one succeeded, switch to the last one.
	char pfilepth[256];
	sprintf(pfilepth, "./USBDev.log");
	FILE* pLocalLog = fopen(pfilepth, "r+b");
	if(pLocalLog == NULL)
	{
		printf("Local log file does not exist. Try to create one.\r\n");
		pLocalLog = fopen(pfilepth, "wb");
	}
	sprintf(pfilepth, "/mnt/sda1/USBDev.log");
	FILE* pRemoteLog = fopen(pfilepth, "r+b");
	if(pRemoteLog == NULL)
	{
		printf("Remote log file does not exist. Try to create one.\r\n");
		pRemoteLog = fopen(pfilepth, "wb");
	}
	if(pRemoteLog != NULL)
	{
		printf("Remote log file selected.\r\n");
		if(pLocalLog != NULL)
			fclose(pLocalLog); 
		pLogFile = pRemoteLog;
		nLogFileIndex = 1;
	}
	else
	{
		printf("Local log file selected.\r\n");
		nLogFileIndex = 0;
		pLogFile = pLocalLog;
	}
	//	If log file opened, seek to the end.
	if(pLogFile != NULL)
	{
		fseek(pLogFile, 0, SEEK_END);
		//	Insert some endlines
		char *pendlines = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";
		fwrite(pendlines, strlen(pendlines), 1, pLogFile);
	}
	//	Create Semaphore
	m_pLogFileLock = CreateLock();
	m_LogTmStart = DEV_GetCurrentMilliSecond();
}

void AcquireAllLocks()
{
	//	Firstly lock all 
	for(int i = 0; i < MAX_DEV_CNT; i++)
	{
		for(int j = 0; j < MAX_STREAM_CNT; j++)
		{
			for(int k = 0; k < MAX_VIRTCH_CNT; k++)
				AcquireLock(gUsbStreamLock[i][j][k], 0xFFFFFFFF);
		}
		AcquireLock(gUsbCmdLock[i], 0xFFFFFFFF);
	}
}

void ReleaseAllLocks()
{
	//	Release all lock at end 
	for(int i = 0; i < MAX_DEV_CNT; i++)
	{
		for(int j = 0; j < MAX_STREAM_CNT; j++)
		{
			for(int k = 0; k < MAX_VIRTCH_CNT; k++)
				ReleaseLock(gUsbStreamLock[i][j][k]);
		}
		ReleaseLock(gUsbCmdLock[i]);
	}
}

int DEV_WriteLogFile(UINT32 logtype, UINT32 devindex, BYTE* logstr)
{
	if(pLogFile == NULL)
		return DEV_NOTAVAILABLE;
	//	Log data
	UINT64 ltNow = DEV_GetCurrentMilliSecond();
	char pLog[4096] = "\0";
	int loglen = strlen((char*)logstr);
	bool bEndl = (logstr[loglen - 1] == '\n');
	USBDev_Time time = DEV_GetCurrentTime();
	char pTime[256] = "\0";
	sprintf(pTime, "[%04d:%02d:%02d %02d:%02d:%02d.%03d]\t", time.year, time.month, time.day, time.hour, time.minute, time.second, time.millisecond);
	//	0 for global log, and 1 for per device log.
	if(logtype == 0)
		sprintf(pLog, "%sLIB  %s%s", pTime, logstr, bEndl ? "" : "\r\n");
	else
		sprintf(pLog, "%sDEVICE %d  %s%s", pTime, devindex, logstr, bEndl ? "" : "\r\n");
	AcquireLock(m_pLogFileLock, 0xFFFFFFFF);
	long filesz = ftell(pLogFile);
	if(filesz >= MAX_LOGFILE_SZ)
	{
		fseek(pLogFile, 0, SEEK_SET);
	}
	fwrite(pLog, 1, strlen(pLog), pLogFile);
	fflush(pLogFile);
	ReleaseLock(m_pLogFileLock);
	return DEV_SUCCESS;
}

int PopCount64(UINT64 t);

DEV_STATUS DEV_OpenDevice()
{
	char pbuf[512];

	if(g_bDeviceClosed == false)
	{
		sprintf(pbuf, "File USBDev.cpp Line %5d: The library is already opened. The API will quit.\r\n", __LINE__);
		printf("%s", pbuf);
		DEV_WriteLogFile(0, 0, (BYTE*)pbuf);
		return DEV_ALREADYOPENED;
	}

	g_bBypassAllCmd = false;

	AcquireAllLocks();

USBDev_RETRY_OPEN_ONCE:

	DEV_STATUS stat = OpenDevice();

	if(stat == DEV_DEVICENOTFOUND)
	{
		sprintf(pbuf, "File USBDev.cpp Line %5d: Device not found. Abort open device procedure.\r\n", __LINE__);
		printf("%s", pbuf);
		DEV_WriteLogFile(0, 0, (BYTE*)pbuf);
		ReleaseAllLocks();
		return DEV_DEVICENOTFOUND;
	}
	else if(stat == DEV_SUCCESS)
	{
		sprintf(pbuf, "File USBDev.cpp Line %5d: Device recovered successfully.\r\n", __LINE__);
		printf("%s", pbuf);
		DEV_WriteLogFile(0, 0, (BYTE*)pbuf);
	}

	printf("Line %5d: Begin open device(s).\r\n", __LINE__);

	printf("Line %5d: Release all locks.\r\n", __LINE__);

	ReleaseAllLocks();

	if(DEV_SUCCESS != stat)
	{
		DEV_CloseDevice();
		if(DEV_UNAUTHORIZED == stat)
			printf("File USBDev.cpp Device not authorized. You may need to contact local sales.\r\n");
		else
		{
			goto USBDev_RETRY_OPEN_ONCE;
		}
	}
	else
	{
		printf("File USBDev.cpp Line %5d: Device opened successfully.\r\n", __LINE__);
	}	

	return stat;
}

DEV_STATUS OpenDevice()
{
	int nCurrentOpenedDevice = 0;
	printf("File USBDev.cpp Last device open status: %d\r\n", g_bDeviceClosed);
	if(g_bDeviceClosed == false)
	{
		//	If device already opened successfully, return immediately and do nothing.
		printf("File USBDev.cpp Device already opened. Return.\r\n");
		return DEV_SUCCESS;
	}

	//	Open device.
	//printf("Query once.\r\n");
	int nDevCount = DEV_QueryDevice();
	printf("File USBDev.cpp Query done. Total device count: %d\r\n", nDevCount);
	DEV_STATUS stat = DEV_SUCCESS;

	printf("File USBDev.cpp Line %5d: DEV_OpenDevice Open Status: %d %08x\r\n", __LINE__, stat, nCurrentOpenedDevice);

	//	Open each device.
	//	If a device is unauthorized, and slot is required, return DEV_UNAUTHORIZED.
	for(int i = 0; i < nDevCount; i++)
	{
		//	Create a new one and verify its magic number.
		USBController* pNewUsb = new USBController();
		//printf("Try open new USB device.\r\n");
		//	Device open if put to slot or set to capturing mode.
		stat = pNewUsb->OpenDevice(i, g_bOpenDeviceOnly ? 2 : (g_bPutDeviceSlot | g_bInitForCapturing));
		//printf("New device opened.\r\n");

		if(gbPrintDebugInfo >= LOG_CRITICAL)
		{
			char pbuf[512];
			sprintf(pbuf, "File USBDev.cpp Device %d open status: %d\r\n", i, stat);
			printf("%s", pbuf);
			DEV_WriteLogFile(0, 0, (BYTE*)pbuf);
		}

		//	If initialize device for capturing, check return status. Else do not need to check.
		//if((DEV_SUCCESS != stat) && (g_bPutDeviceSlot | g_bInitForCapturing))
		if(DEV_SUCCESS != stat)
		{
			//printf("Deleting temporary USB device.\r\n");
			delete pNewUsb;
			//printf("Finished deleting temporary USB device.\r\n");
			pNewUsb = NULL;
			//	The problem must be solved before opening all devices.
			break;
		}
		//	Now do not clear device status.
		//if(DEV_UNAUTHORIZED == stat)
		//{
		//	nCurrentOpenedDevice = 0;
		//	break;
		//}

		if(pNewUsb)
		{
			UINT32* uMagic = pNewUsb->m_nMagicDataBuf;
			UINT32 pos = i;

			if(pNewUsb)
			{
				gUsbController[pos] = pNewUsb;
				nCurrentOpenedDevice++;
			}
		}

	}

	int nDevOpened = 0;

	//	Initialize device(s) only if succeeded.
	if((DEV_SUCCESS == stat) && (DEV_UNAUTHORIZED != stat))
	{
		for(int i = 0; i < MAX_DEV_CNT; i++)
		{
			if(gUsbController[i])
			{
				gUsbController[i]->InitDevice();
				nDevOpened++;
			}
		}
	}

	printf("File USBDev.cpp Line %5d: DEV_OpenDevice Open Status: %d %08x\r\n", __LINE__, stat, nCurrentOpenedDevice);

	if((DEV_SUCCESS == stat) && nCurrentOpenedDevice)
	{
		g_bDeviceClosed = false;
		g_thHardwareMonitor = NULL;
	}
	else
	{
		//DEV_CloseDevice();
		//stat = DEV_DEVICENOTFOUND;
	}
	printf("File USBDev.cpp Line %5d: Last device open status: %d\r\n", __LINE__, g_bDeviceClosed);
	return stat;
}

DEV_STATUS DEV_CloseDevice()
{
	//printf("Line %5d: Calling DEV_CloseDevice\r\n", __LINE__);

	g_bDeviceClosed = true;

	//	Wait for hardware monitor thread to close.
	if(g_thHardwareMonitor != NULL)
	{
		WaitForThread(g_thHardwareMonitor, 0xFFFFFFFF);
		ThreadSleep(50);
		CloseThread(g_thHardwareMonitor);
		g_thHardwareMonitor = NULL;
	}

	AcquireAllLocks();

	for(int i = 0; i < MAX_DEV_CNT; i++)
	{
		if(gUsbController[i])
		{
			gUsbController[i]->CloseDevice();
			delete gUsbController[i];
			gUsbController[i] = NULL;
		}
	}

	ReleaseAllLocks();

	CloseDevices();

	return DEV_SUCCESS;
}

DEV_STATUS DEV_LocalGetFrame(UINT32 uDeviceIndex, UINT32 uStreamIndex, UINT32 uVirtualChIndex, DEV_FRAME_INFO* pFrameInfo, UINT32 bIsCopy, UINT32 uMaxBufSize)
{
	//	Clear frame info
	memset(pFrameInfo, 0, sizeof(DEV_FRAME_INFO) - 4);

	if(uDeviceIndex >= MAX_DEV_CNT)
		return DEV_INVALIDPARAM;
	if(uStreamIndex >= MAX_STREAM_CNT)
		return DEV_INVALIDPARAM;
	if((uVirtualChIndex & 0xFFFF) >= MAX_VIRTCH_CNT)
		return DEV_INVALIDPARAM;

	if(g_bBypassAllCmd)
	{
		ThreadSleep(100);
		return DEV_NOTAVAILABLE;
	}

	DEV_FRAME_INFO fiLocal;
	memset(&fiLocal, 0, sizeof(DEV_FRAME_INFO));
	DEV_STATUS status = DEV_FAIL;

	AcquireLock(gUsbStreamLock[uDeviceIndex][uStreamIndex][(uVirtualChIndex & 0xFFFF)], 0xFFFFFFFF);

	if(gUsbController[uDeviceIndex] != NULL)
	{
		int uImageWidth = 0, uImageHeight = 0;
		BYTE* pBuffer = gUsbController[uDeviceIndex]->GetFrame(uStreamIndex, uVirtualChIndex, &uImageWidth, &uImageHeight, &fiLocal);
		if(g_bDeviceClosed)
			pBuffer = NULL;
		if(pBuffer == NULL)
			memset(pFrameInfo, 0, sizeof(DEV_FRAME_INFO) - 4);

		//	Do bin if necessary
		if(gUsbController[uDeviceIndex]->m_n034BinMode > 0)
		{
			if(gUsbController[uDeviceIndex]->m_n034BinMode == 1)
			{
				//	Bin 2
				for(int i = 0; i < uImageWidth * uImageHeight / 2; i++)
				{
					fiLocal.pFrameBuffer[i] = (fiLocal.pFrameBuffer[(i << 1) + 0] + fiLocal.pFrameBuffer[(i << 1) + 1]) >> 1;
				}
				fiLocal.nFrameWidth = fiLocal.nFrameWidth / 2;
			}
			else if(gUsbController[uDeviceIndex]->m_n034BinMode == 2)
			{
				//	Bin 4
				for(int i = 0; i < uImageWidth * uImageHeight / 4; i++)
				{
					fiLocal.pFrameBuffer[i] = (fiLocal.pFrameBuffer[(i << 2) + 0] + fiLocal.pFrameBuffer[(i << 2) + 1] + fiLocal.pFrameBuffer[(i << 2) + 2] + fiLocal.pFrameBuffer[(i << 2) + 3]) >> 2;
				}
				fiLocal.nFrameWidth = fiLocal.nFrameWidth / 4;
			}
		}
	}

	ReleaseLock(gUsbStreamLock[uDeviceIndex][uStreamIndex][(uVirtualChIndex & 0xFFFF)]);

	//	If not copy, push frame buffer out. Else do verify and copy buffer immediately. Else copy the full buffer.
	if(bIsCopy)
	{
		BYTE* ptr = pFrameInfo->pFrameBuffer;
		*pFrameInfo = fiLocal;
		pFrameInfo->pFrameBuffer = ptr;

		//	If overflow, set error state.
		//	Else if there's valid buffer, copy and set valid flag.
		if(fiLocal.nFrameWidth * fiLocal.nFrameHeight > uMaxBufSize)
		{
			status = DEV_BUFOVERFLOW;
			pFrameInfo->bFrameValid = false;
		}
		else if(pFrameInfo->pFrameBuffer != NULL)
		{
			//	Set corresponding states.
			memcpy(pFrameInfo->pFrameBuffer, fiLocal.pFrameBuffer, fiLocal.nFrameWidth * fiLocal.nFrameHeight);
			pFrameInfo->bFrameValid = true;
			status = DEV_SUCCESS;
		}
		else
		{
			status = DEV_BUFINVALID;
			pFrameInfo->bFrameValid = false;
		}
	}
	else
	{
		memcpy(pFrameInfo, &fiLocal, sizeof(DEV_FRAME_INFO) - 4);
		//*pFrameInfo = fiLocal;
		if(pFrameInfo->pFrameBuffer != NULL)
		{
			//	Set valid if there's valid buffer in the packet.
			//pFrameInfo->bFrameValid = true;
			status = DEV_SUCCESS;
		}
		else
		{
			//pFrameInfo->bFrameValid = false;
		}
	}

	return status;
}

DEV_STATUS DEV_GetFrame(UINT32 uDeviceIndex, UINT32 uStreamIndex, UINT32 uVirtualChIndex, DEV_FRAME_INFO* pFrameInfo)
{
	DEV_STATUS status = DEV_LocalGetFrame(uDeviceIndex, uStreamIndex, uVirtualChIndex, pFrameInfo, FALSE, 0);
	return status;
}

char* DEV_GetLastError()
{
	return NULL;
}

DEV_STATUS DEV_SetDeviceParameter(UINT32 uDeviceMask, UINT32 nParamType, UINT32* uParamValue)
{
	int nDeviceSel[MAX_DEV_CNT] = {0};
	int n = 1;
	int i;
	DEV_PARAM uParamType = (DEV_PARAM)nParamType;
	for(i = 0; i < MAX_DEV_CNT; i++)
	{
		if(uDeviceMask & n)
			nDeviceSel[i] = 1;
		n = n << 1;
	}

	UINT64 nStart, nEnd;

	//	Global commands are executed outside.
	bool b_isgcmd = false;
	switch(uParamType)
	{
	case SET_LOG_LEVEL:
		gbPrintDebugInfo = uParamValue[0];
		b_isgcmd = true;
		break;
	default:
		break;
	}
	if(b_isgcmd)
		return DEV_SUCCESS;

	if(g_bBypassAllCmd)
	{
		ThreadSleep(100);
		return DEV_NOTAVAILABLE;
	}

	//	Device specific commands.
	DEV_STATUS statret = DEV_UNSUPPORTED;
	UINT32 uResp[256];
	memset(uResp, 0, 1024);
	for(i = 0; i < MAX_DEV_CNT; i++)
	{
		if(nDeviceSel[i] && gUsbController[i])
		{
			AcquireLock(gUsbCmdLock[i], 0xFFFFFFFF);
			if(gUsbController[i])
			{
				switch(uParamType)
				{
				case SET_TRIGGER_MODE:
					statret = gUsbController[i]->SetTriggerMode(uParamValue[0]);
					break;
				case SET_TRIGGER_PERIOD:
					statret = gUsbController[i]->SetTriggerInterval(uParamValue[0]);
					break;
				case SET_TRIGGER_ONCE:
					statret = gUsbController[i]->SetTriggerOnce(uParamValue[0]);
					break;
				case SET_TRIGGER_PIN_MODE:
					statret = gUsbController[i]->SetTriggerPinMode(uParamValue[0]);
					break;

				case SET_LED_MODE:
					statret = gUsbController[i]->Set034LEDMode(uParamValue[0]);
					break;
				case SET_LED_POLARY:
					statret = gUsbController[i]->Set034LEDPolary(uParamValue[0]);
					break;
				case SET_LED_ON_TIME:
					statret = gUsbController[i]->Set034LEDOnTime(uParamValue[0]);
					break;

				case SET_SENSOR_REG:
					//	[0] Bus  [1] Slave Addr  [2] Reg Addr  [3] Reg Data  [4] RA Len  [5] DA Len
					statret = gUsbController[i]->WriteRegister(uParamValue[1], uParamValue[2], uParamValue[3], uParamValue[4], uParamValue[5], uParamValue[0]);
					break;

				case SET_034_ANALOG_GAIN:
					//	[0] AGC Enable  [1] Gain  [2] Max Gain for AGC
					statret = gUsbController[i]->Set034AnalogGain(uParamValue[0], uParamValue[1], uParamValue[2]);
					break;
				case SET_034_EXPOSURE:
					//	[0] AEC Enable  [1] HDR Enable  [2] Auto Exposure Knee Adjust Enable  [3] Coarse Int  [4] Knee 1  [5] Knee 2  [6] Min Int for AEC  [7] Max Int for AEC
					statret = gUsbController[i]->Set034Exposure(uParamValue[0], uParamValue[1], uParamValue[2], uParamValue[3], uParamValue[4], uParamValue[5], uParamValue[6], uParamValue[7]);
					break;
				case SET_034_DESIREDBIN:
					statret = gUsbController[i]->Set034DesiredBin(uParamValue[0]);
					break;
				case SET_034_RESET:
					statret = gUsbController[i]->Set034Reset(uParamValue[0]);
					break;

				case SET_034_BINMODE:
					statret = gUsbController[i]->Set034BinMode(uParamValue[0]);
					break;
				case SET_034_RESETPIN:
					statret = gUsbController[i]->Set034ResetPin(uParamValue[0]);
					break;
				case SET_034_INITIALIZE:
					statret = gUsbController[i]->Set034Initialize();
					break;

				case SET_PWM_MODE:
					statret = gUsbController[i]->SetPWMMode(uParamValue[0], uParamValue[1], uParamValue[2], uParamValue[3]);
					break;

				default:
					break;
				}
			}
			ReleaseLock(gUsbCmdLock[i]);

		}
	}
	//	If the operation fails, sleep for 50 ms.
	if(DEV_SUCCESS != statret)
		ThreadSleep(50);
	return statret;
}

#ifndef LIBVER
#	error "Library version (LIBVER) must be defined as a string! Expected is in x.x.x.x format!"
#endif

bool ParseVersionCodeStr(char* pstr, UINT32* pver)
{
	char pbuf[512];
	sprintf(pbuf, "File USBDev.cpp Line %5d: The version code is %s.\r\n", __LINE__, pstr);
	printf("%s", pbuf);
	DEV_WriteLogFile(0, 0, (BYTE*)pbuf);
	int plen = strlen(pstr);
	int ver = 0;
	int i;
	for(i = 0; i < plen; i++)
	{
		if(pstr[i] >= '0' && pstr[i] <= '9')
		{
			ver *= 10;
			ver += (pstr[i] - '0');
		}
		else if(pstr[i] == '.')
		{
			*pver = ver;
			pver++;
			ver = 0;
		}
		if(i == plen - 1)
		{
			*pver = ver;
		}
	}
	if(i == plen)
		return true;
	else
		return false;
}

DEV_STATUS DEV_GetDeviceParameter(UINT32 uDeviceMask, UINT32 nParamType, UINT32* uParamValue)
{
	int nDeviceSel[MAX_DEV_CNT] = {0, 0, 0, 0, 0, 0, 0, 0};
	int n = 1;
	int i;
	DEV_PARAM uParamType = (DEV_PARAM)nParamType;
	for(i = 0; i < MAX_DEV_CNT; i++)
	{
		if(uDeviceMask & n)
			nDeviceSel[i] = 1;
		n = n << 1;
	}
	DEV_STATUS statret = DEV_UNSUPPORTED;

	//	Global commands are executed outside.
	bool b_isgcmd = false;
	switch(uParamType)
	{
	default:
		break;
	}
	if(b_isgcmd)
		return statret;

	if(g_bBypassAllCmd)
	{
		ThreadSleep(100);
		return DEV_NOTAVAILABLE;
	}

	for(i = 0; i < MAX_DEV_CNT; i++)
	{
		if(nDeviceSel[i] && gUsbController[i])
		{
			AcquireLock(gUsbCmdLock[i], 0xFFFFFFFF);
			if(gUsbController[i])
			{
				switch(uParamType)
				{
				case GET_SENSOR_REG:
					//	[0] Bus  [1] Slave Addr  [2] Reg Addr  [3] Dummy  [4] RA Len  [5] DA Len
					statret = gUsbController[i]->ReadRegister(uParamValue[1], uParamValue[2], uParamValue[4], uParamValue[5], uParamValue, uParamValue[0]);
					break;
					
				case GET_034_AECAGCSTATUS:	//	Get 034 AEC / AGC Status. Bit 0 AEC  Bit 1 AGC
					statret = gUsbController[i]->Get034AECAGCStatus(uParamValue);
					break;
				case GET_034_AECMINMAXINT:	//	Get 034 Min / Max Integration. [0] Min  [1] Max
					statret = gUsbController[i]->Get034AECMinMaxCoarseInteg(uParamValue);
					break;
				case GET_034_TOTALINTEG:	//	Get 034 Total Integration. [0] Integration
					statret = gUsbController[i]->Get034TotalInteg(uParamValue);
					break;
				case GET_034_KNEEPOINTS:	//	Get 034 Knee Points (Exposure) (HDR only) [0] Knee 1  [1] Knee 2
					statret = gUsbController[i]->Get034KneePoints(uParamValue);
					break;
				case GET_034_AGC_MAXGAIN:	//	Get 034 AGC Max Gain. [0] Gain
					statret = gUsbController[i]->Get034AGCMaxGain(uParamValue);
					break;
				case GET_034_ANALOGGAIN:	//	Get 034 Analog Gain. [0] Gain
					statret = gUsbController[i]->Get034AnalogGain(uParamValue);
					break;
				case GET_034_DESIREDBIN:	//	Get 034 Desired Bin. [0] Bin
					statret = gUsbController[i]->Get034DesiredBin(uParamValue);
					break;
				case GET_034_HDRSTATE:		//	Get 034 HDR State. 0: Disabled  [1] Enabled
					statret = gUsbController[i]->Get034HDRState(uParamValue);
					break;
				case GET_034_KNEEAUTOADJ:
					statret = gUsbController[i]->Get034KneeAutoAdjust(uParamValue);
					break;

				default:
					*uParamValue = 0;
					break;
				}
			}
			ReleaseLock(gUsbCmdLock[i]);
		}
	}
	//	If the operation fails, sleep for 50 ms.
	if(DEV_SUCCESS != statret)
		ThreadSleep(50);
	return statret;
}


#include <stdio.h>


class __InitSignals
{
public:
	__InitSignals(){InitSignals();}
};

__InitSignals __init;

