#include <stdio.h>

#include "../USBDev.h"

#include "../Misc/USBDev_Threads.h"
#include "../Misc/USBDev_Semaphore.h"

#include "../USBController.h"

#include "../Bayer2RGBA.h"


volatile bool bThreadClose = false;

void* pmutex = NULL;
void* pevent = NULL;

volatile bool bThreadPause = true;

void* ThreadTest(void* param)
{
	int i = (int)param;
	printf("Thread started: %d\r\n", i);
	DEV_FRAME_INFO FrameInfo;
	while(1)
	{
		if(bThreadClose)
			break;
		if(bThreadPause)
		{
			ThreadSleep(100);
			continue;	
		}
		//break;

		/*AcquireLock(pmutex, 0xFFFFFFFF);
		//printf("%d\r\n", i);
		ReleaseLock(pmutex);

		int err = WaitForEvent(pevent, 500);
		if(err == 0)
		{
		printf("Thread %d activated.\r\n", i);
		}*/
		//DEV_STATUS status = DEV_SirModuleGetIrisImage(i, 0x00000000, &FrameInfo);
		//printf("Thread %d get frame %s.\r\n", i, status ? "Failed" : "Succeeded");
	}
	printf("Thread stopped: %d\r\n", i);
	return NULL;
}

volatile bool g_bPausePLLThread = true;

void* ThreadTestPLL(void* param)
{
	UINT32 uPLLStatus[6] = {0};
	UINT32 uParam[2] = {0};
	while(1)
	{
		if(bThreadClose)
			break;
		if(g_bPausePLLThread)
		{
			ThreadSleep(100);
			continue;
		}
		bool bUnlink = false;
		for(int i = 0; i < 8; i++)
		{
			uParam[0] = i;
			uParam[1] = 0;
			//DEV_GetDeviceParameter(0xFFFF, GET_PLL_STATUS_DATA, uParam);
			if(uParam[0] != uPLLStatus[i])
				bUnlink = true;
			//printf("PLL Status Word %d is %08x.\n", i, uParam[0]);
			uPLLStatus[i] = uParam[0];
		}
		if(bUnlink)
		{
			for(int i = 0; i < 6; i++)
			{
				printf("%08x ", uPLLStatus[i]);
			}
			printf("\r\n");
		}
	}
	return NULL;
}

void* ThreadCapture(void* param)
{
	int i = (int)param;
	i = 2;
	printf("Capture thread started: %d\r\n", i);
	DEV_FRAME_INFO FrameInfo;
	int nSkipFrames = 5;
	while(1)
	{
		if(bThreadClose)
			break;
		if(bThreadPause)
		{
			ThreadSleep(100);
			nSkipFrames = 5;
			continue;	
		}
		DEV_STATUS status = DEV_FAIL;
		//if(i == 0)
		//	status = DEV_SirModuleGetRGBImage(&FrameInfo);
		//else if(i == 1)
		//	status = DEV_SirModuleGetDepthImage(&FrameInfo);
		//else if(i == 2)
		//	status = DEV_SirModuleGetIrisImage(0, 0xFFFFFFFF, &FrameInfo);
		//else if(i == 3)
		//	status = DEV_SirModuleGetIrisImage(1, 0xFFFFFFFF, &FrameInfo);
		//printf("Thread %d get frame %s.\r\n", i, status ? "Failed" : "Succeeded");
		/*if(nSkipFrames)
			nSkipFrames--;
		if(nSkipFrames == 0 && DEV_SUCCESS == status)
		{
			char pfn[2048];
			char phdr[8];
			if(i == 0)
				memcpy(phdr, "face", 5);
			else if(i == 1)
				memcpy(phdr, "tof", 4);
			else if(i == 2)
				memcpy(phdr, "iris0", 6);
			else if(i == 3)
				memcpy(phdr, "iris1", 6);
			sprintf(pfn, "/capture/0/%s%d.bin", phdr, FrameInfo.nTimeStamp);
			SaveRawBayer(FrameInfo.pFrameBuffer, pfn, FrameInfo.nFrameWidth / 2, FrameInfo.nFrameHeight);
		}*/
	}
	printf("Thread stopped: %d\r\n", i);
	return NULL;
}

int parse_int(char* p)
{
	int value = 0;
	while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	while(*p)
	{
		if(*p >= '0' && *p <= '9')
		{
			value *= 10;
			value += *p - '0';
		}
		else
			break;
		p++;
	}
	return value;
}

void init_led()
{
	UINT32 uParam[2];
	UINT32 uLEDData[512] = {0};

	printf("Prepare led data\r\n");

	//	Make an RGB loop.
	for(int i = 0; i < 32; i++)
		uLEDData[i] = 0xFFFF00FF | ((((32 - i) * 80) / 32 + 16) << 8);
	for(int i = 32; i < 64; i++)
		uLEDData[i] = 0xFFFF00FF | ((((i - 32) * 80) / 32 + 16) << 8);
	for(int i = 64; i < 96; i++)
		uLEDData[i] = 0xFF00FFFF | ((((96 - i) * 80) / 32 + 16) << 16);
	for(int i = 96; i < 128; i++)
		uLEDData[i] = 0xFF00FFFF | ((((i - 96) * 80) / 32 + 16) << 16);
	for(int i = 128; i < 160; i++)
		uLEDData[i] = 0x80FFFFFF | ((((160 - i) * 80) / 32 + 16) << 24);
	for(int i = 160; i < 192; i++)
		uLEDData[i] = 0x80FFFFFF | ((((i - 160) * 80) / 32 + 16) << 24);
	for(int i = 192; i < 224; i++)
		uLEDData[i] = 0xFF0000FF | ((((224 - i) * 80) / 32 + 16) << 8) | ((((224 - i) * 80) / 32 + 16) << 16);
	for(int i = 224; i < 256; i++)
		uLEDData[i] = 0xFF0000FF | ((((i - 224) * 80) / 32 + 16) << 8) | ((((i - 224) * 80) / 32 + 16) << 16);
	for(int i = 256; i < 288; i++)
		uLEDData[i] = 0x80FF00FF | ((((288 - i) * 80) / 32 + 16) << 8) | ((((288 - i) * 80) / 32 + 16) << 24);
	for(int i = 288; i < 320; i++)
		uLEDData[i] = 0x80FF00FF | ((((i - 288) * 80) / 32 + 16) << 8) | ((((i - 288) * 80) / 32 + 16) << 24);
	for(int i = 320; i < 352; i++)
		uLEDData[i] = 0x8000FFFF | ((((352 - i) * 80) / 32 + 16) << 16) | ((((352 - i) * 80) / 32 + 16) << 24);
	for(int i = 352; i < 384; i++)
		uLEDData[i] = 0x8000FFFF | ((((i - 352) * 80) / 32 + 16) << 16) | ((((i - 352) * 80) / 32 + 16) << 24);
	for(int i = 384; i < 416; i++)
		uLEDData[i] = 0x800000FF | ((((416 - i) * 80) / 32 + 16) << 8) | ((((416 - i) * 80) / 32 + 16) << 16) | ((((416 - i) * 80) / 32 + 16) << 24);
	for(int i = 416; i < 448; i++)
		uLEDData[i] = 0x800000FF | ((((i - 416) * 80) / 32 + 16) << 8) | ((((i - 416) * 80) / 32 + 16) << 16) | ((((i - 416) * 80) / 32 + 16) << 24);
	for(int i = 448; i < 512; i++)
		uLEDData[i] = 0xFFFFFFFF;


	printf("Write led data\r\n");
	for(int i = 0; i < 512; i += 64)
	{
		//	Set page (128)
		uParam[0] = 0x0180;
		uParam[1] = i / 64;
		//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
		for(int j = i; j < i + 64; j++)
		{
			uParam[0] = 0x0100 | ((j - i) * 2);
			uParam[1] = uLEDData[j] & 0xFFFF;
			//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
			uParam[0] = 0x0100 | ((j - i) * 2 + 1);
			uParam[1] = (uLEDData[j] >> 16) & 0xFFFF;
			//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
		}
	}
	printf("Led done.\r\n");
}

void InvertLEDDriverOutput(int invout)
{
	UINT32 uParam[2];
	uParam[0] = 0x0100 | (137);
	uParam[1] = invout;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
}

/* pwminterval: PWM interval per 1/100 cycle. In 1 us.
 * stepinterval: Interval between 2 steps. In 10 us.
 * default_index: Default profile start index.
 * default_length: Default profile step length.
 * index: Current profile start index.
 * length: Current profile step length.
 * loopcnt: Number of loops.
 * default_en: Enable default profile.
 * invout: Invert outputs. Set to 0x0F for external LED bar, and 0x00 for internal LED bar.
 * start: Request a new action.
 */
void SetLEDDriverCommand(int pwminterval, int stepinterval, int default_index, int default_length, int index, int length, int loopcnt, int default_en, int start)
{
	//UINT32 uParam[2];
	////	129:r_soc_pwminterval 130:r_soc_stepinterval 131:r_soc_default_startindex 132:r_soc_default_numsteps
	////	133: r_soc_startindex 134:r_soc_numsteps 135:r_soc_loopcnt 136:r_soc_startstop
	//uParam[0] = 0x0100 | (129);
	//uParam[1] = pwminterval;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//uParam[0] = 0x0100 | (130);
	//uParam[1] = stepinterval;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//uParam[0] = 0x0100 | (131);
	//uParam[1] = default_index;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//uParam[0] = 0x0100 | (132);
	//uParam[1] = default_length;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//uParam[0] = 0x0100 | (133);
	//uParam[1] = index;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//uParam[0] = 0x0100 | (134);
	//uParam[1] = length;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//uParam[0] = 0x0100 | (135);
	//uParam[1] = loopcnt;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//uParam[0] = 0x0100 | (136);
	//uParam[1] = (default_en << 15) | start;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
}

void init_motor()
{
	UINT32 uParam[2];
	UINT32 uMotorData[] = { 
		0x00, 0x01, 0x03, 0x02, 0x00, 0x00, 0x02, 0x03,		//	Pure black
		0x01, 0x00, 0x00, 0x04, 0x0C, 0x08, 0x00, 0x00,		//	Red 
		0x08, 0x0C, 0x04, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,		//	Green
		0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Blue
		0xF9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Red - Green
		0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Red - Blue
		0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Green - Blue
		0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Red - Green - Blue
	};
	UINT32 uMotorEn[] = { 
		0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,		//	Pure black
		0x03, 0x03, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,		//	Red 
		0x0C, 0x0C, 0x0C, 0x0C, 0xFF, 0xFF, 0xFF, 0xFF,		//	Green
		0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Blue
		0xF9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Red - Green
		0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Red - Blue
		0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Green - Blue
		0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,		//	Red - Green - Blue
	};
	printf("Write motor data\r\n");
	for (int i = 0; i < 64; i++)
	{
		uParam[0] = (UINT32)(0x00 | i);
		uParam[1] = uMotorData[i];
		//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
		uParam[0] = (UINT32)(0x40 | i);
		uParam[1] = uMotorEn[i];
		//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	}
	printf("motor done\r\n");
}

/* startindex: Start index 
 * steplength: Number of steps for one run
 * loopcount: Number of loops
 * delayunit: Delay per step (in 1 us)
 */
void SetMotorDriverCommand(int startindex, int steplength, int loopcount, int delayunit)
{
	//printf("begin send command\r\n");
	//UINT32 uParam[2];
	//int startslot = 0;
	////	Set slot number
	//uParam[0] = 193;
	//uParam[1] = (UINT32)startslot;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	////	Set slot index
	//uParam[0] = 194;
	//uParam[1] = (UINT32)startindex;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	////	Set step length
	//uParam[0] = 195;
	//uParam[1] = (UINT32)steplength;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	////	Set delay unit
	//uParam[0] = 196;
	//uParam[1] = (UINT32)delayunit;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	////	Set loop count
	//uParam[0] = 197;
	//uParam[1] = (UINT32)loopcount;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	////	Start
	//uParam[0] = 198;
	//uParam[1] = 0x00000002;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	////	Stop
	//uParam[0] = 198;
	//uParam[1] = 0x00000001;
	//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uParam);
	//printf("End send command\r\n");
}

/*void WaitForMotorIdle()
{
	UINT32 uStatus = 0;
	while(1)
	{
		DEV_GetDeviceParameter(0xFFFF, GET_MOTOR_STATUS, &uStatus);
		if((uStatus & 0x11) == 0x11)
			break;
	}
}

void ScanForMotorTerminal(int index, int term)
{
	//	Scan for motor terminal
	while(1)
	{
		UINT32 uData[2] = {0x0202 + (index << 8), 0x0003};
		UINT32 uStatus = 0;
		DEV_GetDeviceParameter(0xFFFF, GET_MOTOR_STATUS, &uStatus);
		if((uStatus & term) != 0)
			break;
		DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
		WaitForMotorIdle();
	}
}

int ScanForMotorPosition(int index, int posi)
{
	int steps = 0;
	UINT32 uStatus = 0;
	DEV_GetDeviceParameter(0xFFFF, GET_MOTOR_STATUS, &uStatus);
	//	Check for start position. If in previous position, do init. Else skip.
	bool bSkip = true;
	if((uStatus & posi) != 0)
		bSkip = false;
	if(!bSkip)
	{
		//	Scan for motor position
		while(1)
		{
			UINT32 uData[2] = {0x0202 + (index << 8), 0x0001};
			steps++;
			DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
			WaitForMotorIdle();
			DEV_GetDeviceParameter(0xFFFF, GET_MOTOR_STATUS, &uStatus);
			if((uStatus & posi) == 0)
				break;
		}
	}
	//	Scan for motor position
	while(1)
	{
		UINT32 uData[2] = {0x0202 + (index << 8), 0x0001};
		steps++;
		DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
		WaitForMotorIdle();
		DEV_GetDeviceParameter(0xFFFF, GET_MOTOR_STATUS, &uStatus);
		if((uStatus & posi) != 0)
			break;
	}
	//	Go for another 2 steps to avoid accidental positioning error.
	for(int i = 0; i < 2; i++)
	{
		UINT32 uData[2] = {0x0202 + (index << 8), 0x0001};
		steps++;
		DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
	}
	return steps;
}

void ScanMotor(UINT32* pStep)
{
	int nTotalSteps = 0;
	//	Scan for top motor terminal
	ScanForMotorTerminal(0, 0x04);
	for(int i = 0; i < 4; i++)
	{
		nTotalSteps += ScanForMotorPosition(0, 0x02);
		pStep[i] = nTotalSteps;
	}
	//	Scan for bottom motor terminal
	nTotalSteps = 0;
	ScanForMotorTerminal(1, 0x40);
	for(int i = 0; i < 4; i++)
	{
		nTotalSteps += ScanForMotorPosition(1, 0x20);
		pStep[4 + i] = nTotalSteps;
	}
}*/

int main()
{
	printf("Test\r\n");
	UINT32 devcnt = DEV_QueryDevice();
	printf("Device queried successfully.\r\n");
	UINT32 loglevel = 99;
	//DEV_SetDeviceParameter(0xFFFF, SET_LOG_LEVEL, &loglevel);
	printf("Device count: %d\r\n", devcnt);
	UINT32 uOpenBehavior = 3;
	//DEV_SetDeviceParameter(0xFFFF, SET_OPEN_BEHAVIOR, &uOpenBehavior);	
	DEV_STATUS status = DEV_OpenDevice();
	printf("Test device opened. Error code is %d\r\n", status);
	BYTE* pbuf;
	UINT32 uWidth = 0, uHeight = 0;
	DEV_FRAME_INFO uFrameInfo;
	status = DEV_GetFrame(0, 0, 2, &uFrameInfo);
	printf("Get WDR Frame: %d %d %d\r\n", status, uFrameInfo.nFrameWidth, uFrameInfo.nFrameHeight);
	//	Device won't close after test operation.
	//printf("%d\n", DEV_CloseDevice());

	pevent = CreateEventObject();
	printf("Wait for event\r\n");
	int ret = WaitForEvent(pevent, 200);
	printf("Exit event: %d\r\n", ret);

	printf("Try to get firmware version.\r\n");

	UINT32 uVer[6] = {0};
	//status = DEV_GetDeviceParameter(0xFFFFFFFF, GET_FIRMWARE_VERSION, uVer);
	printf("Bootloader version: %d.%d\r\n", uVer[0], uVer[1]);
	printf("Firmware version: %d.%d\r\n", uVer[2], uVer[3]);
	printf("Hardware version: %d.%d\r\n", uVer[4], uVer[5]);
	//status = DEV_GetDeviceParameter(0xFFFFFFFF, GET_LIBRARY_VERSION, uVer);
	printf("Library version: %d.%d.%d.%d\r\n", uVer[0], uVer[1], uVer[2], uVer[3]);

	//	Create mutex
	pmutex = CreateLock();
	int thread_cnt = 4;
	void** threads = new void*[thread_cnt + 1];
	for(int i = 0; i < thread_cnt; i++)
	{
		threads[i] = CreateThread((void*)ThreadCapture, (void*)i);
		if(threads[i] == NULL)
			printf("Thread creation failed: %d\r\n", i);
	}
	threads[thread_cnt] = CreateThread((void*)ThreadTestPLL, (void*)0);

	//	Set device to rescue mode only (test memory leakage)
	UINT32 uParam[256] = {0};
	//DEV_SetDeviceParameter(0xFFFF, SET_OPEN_BEHAVIOR, uParam);

	//DEV_STREAM_TYPE devStreamType;

	BYTE* prgbabuf = new BYTE[8192 * 5280 * 4];

	DEV_FRAME_INFO FrameInfo;

	UINT32 nLightEN = 0;

	int nFrameIndex[4] = {0, 0, 0, 0};

	bool bSaveFrame = false;

	int nLEDbarinvert = 0x0F;

	//	Input and test
	while(1)
	{
		//	w r 1 2 3 4 s p q e c o k g h i j z t l m 7 8 d b v
		char ch[512];
		printf("c: close  e: signal\t\t");
		scanf("%s", ch);
		if(ch[0] == 'b')
		{
			//	Rear Focus. Top, Bottom. Clockwise, Reverse. 0~9 steps.
			int motorindex = 0;
			int motordir = 0;
			int motorsteps = 0;
			if(ch[1] == 'b')
				motorindex = 1;
			if(ch[2] == 'r')
				motordir = 1;
			if(ch[3] >= '0' && ch[3] <= '9')
				motorsteps = ch[3] - '0';
			else
			{
				printf("Invalid motor command. \r\n");
				continue;
			}
			//	Init motor
			init_motor();
			//	Call motor data.
			motorindex = motorindex * 10 + motordir * 5;
			printf("Motor index is %d\r\n", motorindex);
			SetMotorDriverCommand(motorindex, 5, motorsteps, 5000);
		}
		if(ch[0] == 'v')
		{
			//	vg -- go
			if(ch[1] == 'g')
			{
				int dist = ch[3] - '0';
				if(dist == 0)
					dist = 1250;
				else if(dist == 1)
					dist = 1200;
				else if(dist == 2)
					dist = 1100;
				else if(dist == 3)
					dist = 1050;
				if(ch[2] == 't')
				{
					//SetRearFocusPos(0, dist);
				}
				else if(ch[2] == 'b')
				{
					//SetRearFocusPos(1, dist);
				}
				//if(ch[2] == 't')
				//{
				//	UINT32 uData[2] = {0x0200, 0};
				//	uData[1] = ch[3] - '0';
				//	DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
				//}
				//else if(ch[2] == 'b')
				//{
				//	UINT32 uData[2] = {0x0300, 0};
				//	uData[1] = ch[3] - '0';
				//	DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
				//}
			}
			//	vs -- scan motor steps
			else if(ch[1] == 's')
			{
				//InitRearFocusMotor();
				//UINT32 uData[2];
				////	Disable auto
				//uData[1] = 0;
				//uData[0] = 0x0203;
				//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
				//uData[0] = 0x0303;
				//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
				////	Begin scan
				//UINT32 uSteps[8] = {0};
				//ScanMotor(uSteps);
				//for(int i = 0; i < 8; i++)
				//	printf("%d ", uSteps[i]);
				//printf("\n");
				////	Write steps
				//for(int i = 0; i < 4; i++)
				//{
				//	uData[0] = 0x0200 | (4 + i);
				//	uData[1] = uSteps[0 + i];
				//	DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
				//	uData[0] = 0x0300 | (4 + i);
				//	uData[1] = uSteps[4 + i];
				//	DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
				//}
				////	Enable auto
				//uData[1] = 1;
				//uData[0] = 0x0203;
				//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
				//uData[0] = 0x0303;
				//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
			}
			//	vr -- read motor status
			//	v0,1,2,3 number -- top cw / ccw / bottom cw / ccw for number steps
			else if(ch[1] == 'r')
			{
				//UINT32 uBR0[2] = {0x0201, 1500};
				//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uBR0);
				//UINT32 uBR1[2] = {0x0301, 1500};
				//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uBR1);
				//UINT32 uData = 0;
				//DEV_GetDeviceParameter(0xFFFF, GET_MOTOR_STATUS, &uData);
				//char pstr[64];
				//sprintf(pstr, "Term0:%d  Pos0:%d  Idle0:%d  Term1:%d  Pos1:%d  Idle1:%d\n", (uData & 0x04) != 0, (uData & 0x02) != 0, (uData & 0x01) != 0, (uData & 0x40) != 0, (uData & 0x20) != 0, (uData & 0x10) != 0);
				//printf(pstr);
				//printf("Motor raw data: 0x%08x\r\n", uData);
			}
			else if(ch[1] >= '0' && ch[1] <= '3')
			{
				int step = parse_int(ch + 2);
				UINT32 uData[2] = {0, 0};
				if(ch[1] == '0')
				{
					uData[0] = 0x0202;
					uData[1] = 0x0003;
				}
				else if(ch[1] == '1')
				{
					uData[0] = 0x0202;
					uData[1] = 0x0001;
				}
				else if(ch[1] == '2')
				{
					uData[0] = 0x0302;
					uData[1] = 0x0003;
				}
				else if(ch[1] == '3')
				{
					uData[0] = 0x0302;
					uData[1] = 0x0001;
				}
				printf("Motor %d will run for %d steps.\r\n", (uData[0] >> 8) - 2, step);
				UINT32 uStatus = 0;
				for(int i = 0; i < step; i++)
				{
					//DEV_SetDeviceParameter(0xFFFF, SET_MOTOR_DRIVER, uData);
					//while(1)
					//{
					//	DEV_GetDeviceParameter(0xFFFF, GET_MOTOR_STATUS, &uStatus);
					//	if((uStatus & 0x11) == 0x11)
					//		break;
					//}
				}
			}
			else
				printf("Invalid input command.\r\n");
		}
		if(ch[0] == 'd')
		{
			UINT32 dbgData[256];
			memset(dbgData, 0, 1024);
			if(ch[1] == '0' || ch[1] == 0)
				dbgData[0] = 0;
			else if(ch[1] == '1')
				dbgData[0] = 1;
			else if(ch[1] == '2')
				dbgData[0] = 2;
			else if(ch[1] == 'f')
				printf("Log debug data to file.\r\n");
			else
			{
				printf("Invalid debug page index.\r\n");
				continue;
			}
			//DEV_GetDeviceParameter(0xFFFF, GET_DBG_DATA, dbgData);
			for(int i = 0; i < 4; i += 4)
			{
				for(int j = i; j < i + 4; j++)
					printf("0x%08x  ", dbgData[j]);
				printf("\n");
			}
			//	For disk dump, check for ch[1] == 'f'. Dump 1024 blocks.
			if(ch[1] == 'f')
			{
				FILE* fpDumpDbg = fopen("DumpDbg.txt", "wb");
				if(fpDumpDbg == NULL)
				{
					printf("Debug dump file not opened.\r\n");
					continue;
				}
				for(int i = 0; i < 8192; i++)
				{
					dbgData[0] = 0;
					//DEV_GetDeviceParameter(0xFFFF, GET_DBG_DATA, dbgData);
					//	Only write the first 4 words
					char sDumpDbg[256] = "";
					for(int j = 0; j < 4; j++)
						sprintf(sDumpDbg + strlen(sDumpDbg), "0x%08x  ", dbgData[j]);
					sprintf(sDumpDbg + strlen(sDumpDbg), "\r\n");
					fwrite(sDumpDbg, strlen(sDumpDbg), 1, fpDumpDbg);
				}
				fclose(fpDumpDbg);
			}
		}
		if(ch[0] == 'w')
		{
			printf("Input register address & data: ");
			int addr = 0, data = 0;
			scanf("%d%d", &addr, &data);
			UINT32 uParam[2];
			uParam[0] = addr;
			uParam[1] = data;
			//DEV_SetDeviceParameter(0xFFFF, SET_USER_CONFIG_DATA, uParam);
		}
		if(ch[0] == 'r')
		{
			printf("Input register address: ");
			UINT32 addr = 0;
			scanf("%d", &addr);
			//DEV_GetDeviceParameter(0xFFFF, GET_USER_CONFIG_DATA, &addr);
			printf("The configuration data is %2x.\r\n", addr);
		}
		if(ch[0] == '7')
		{
			printf("Dump user configuration data? ");
			scanf("%s", ch);
			if(ch[0] == 'y')
			{
				UINT32 addr = 0;
				int dumpsz = 512;
				BYTE bDump[2048];
				for(int i = 0; i < dumpsz; i++)
				{
					addr = i;
					//DEV_GetDeviceParameter(0xFFFF, GET_USER_CONFIG_DATA, &addr);
					bDump[i] = (BYTE)addr;
					printf("%02x ", addr);
					if((i + 1) % 16 == 0)
						printf("\r\n");
				}
				FILE* fpDump = fopen("dump.bin", "wb");
				fwrite(bDump, dumpsz, 1, fpDump);
				fclose(fpDump);
			}
		}
		if(ch[0] == '8')
		{
			printf("Upload user configuration data? ");
			scanf("%s", ch);
			if(ch[0] == 'y')
			{
				FILE* fpDump = fopen("dump.bin", "rb");
				BYTE bDump[512];
				fread(bDump, 512, 1, fpDump);
				fclose(fpDump);
				for(int i = 0; i < 512; i++)
				{	
					UINT32 uParam[2];
					uParam[0] = i;
					uParam[1] = bDump[i];
					//DEV_SetDeviceParameter(0xFFFF, SET_USER_CONFIG_DATA, uParam);
					printf("%02x ", bDump[i]);
					if((i + 1) % 16 == 0)
						printf("\r\n");
				}
			}
		}
		if(ch[0] == '1')
		{
			//DEV_SirModuleGetRGBImage(&FrameInfo);
			if(FrameInfo.pFrameBuffer != NULL)
			{
				printf("Stream index %d virtual channel %d received.\r\n", 1, 2);
				printf("Frame width %d height %d timestamp %d framerate %f\r\n", FrameInfo.nFrameWidth, FrameInfo.nFrameHeight, FrameInfo.nTimeStamp, 1000.0 / ((double)FrameInfo.nFrameCycle / (double)FrameInfo.nCycleResolution));
				if(bSaveFrame)
				{
					char pDumpData[256] = "";
					sprintf(pDumpData, "rgb_%d.bin", nFrameIndex[0]);
					nFrameIndex[0]++;
					FILE* fp = fopen(pDumpData, "wb");
					fwrite(FrameInfo.pFrameBuffer, FrameInfo.nFrameWidth * FrameInfo.nFrameHeight, 1, fp);
					fclose(fp);
				}
			}
			else
			{
				printf("Frame get failed.\r\n");
			}
		}
		if(ch[0] == '2')
		{
			//DEV_SirModuleGetDepthImage(&FrameInfo);
			if(FrameInfo.pFrameBuffer != NULL)
			{
				printf("Stream index %d virtual channel %d received.\r\n", 0, 2);
				printf("Frame width %d height %d timestamp %d framerate %f\r\n", FrameInfo.nFrameWidth, FrameInfo.nFrameHeight, FrameInfo.nTimeStamp, 1000.0 / ((double)FrameInfo.nFrameCycle / (double)FrameInfo.nCycleResolution));
				if(bSaveFrame)
				{
					char pDumpData[256] = "";
					sprintf(pDumpData, "tof_%d.bin", nFrameIndex[1]);
					nFrameIndex[1]++;
					FILE* fp = fopen(pDumpData, "wb");
					fwrite(FrameInfo.pFrameBuffer, FrameInfo.nFrameWidth * FrameInfo.nFrameHeight, 1, fp);
					fclose(fp);
				}
			}
			else
			{
				printf("Frame get failed.\r\n");
			}
		}
		if(ch[0] == '3')
		{
			//DEV_SirModuleGetIrisImage(0, 0, &FrameInfo);
			if(FrameInfo.pFrameBuffer != NULL)
			{
				printf("Stream index %d virtual channel %d received.\r\n", 0, 4);
				printf("Frame width %d height %d timestamp %d framerate %f\r\n", FrameInfo.nFrameWidth, FrameInfo.nFrameHeight, FrameInfo.nTimeStamp, 1000.0 / ((double)FrameInfo.nFrameCycle / (double)FrameInfo.nCycleResolution));
				if(bSaveFrame)
				{
					char pDumpData[256] = "";
					sprintf(pDumpData, "iris0_%d.bin", nFrameIndex[2]);
					nFrameIndex[2]++;
					FILE* fp = fopen(pDumpData, "wb");
					fwrite(FrameInfo.pFrameBuffer, FrameInfo.nFrameWidth * FrameInfo.nFrameHeight, 1, fp);
					fclose(fp);
				}
			}
			else
			{
				printf("Frame get failed.\r\n");
			}
		}
		if(ch[0] == '4')
		{
			//DEV_SirModuleGetIrisImage(1, 0, &FrameInfo);
			if(FrameInfo.pFrameBuffer != NULL)
			{
				printf("Stream index %d virtual channel %d received.\r\n", 0, 4);
				printf("Frame width %d height %d timestamp %d framerate %f\r\n", FrameInfo.nFrameWidth, FrameInfo.nFrameHeight, FrameInfo.nTimeStamp, 1000.0 / ((double)FrameInfo.nFrameCycle / (double)FrameInfo.nCycleResolution));
				if(bSaveFrame)
				{
					char pDumpData[256] = "";
					sprintf(pDumpData, "iris1_%d.bin", nFrameIndex[3]);
					nFrameIndex[3]++;
					FILE* fp = fopen(pDumpData, "wb");
					fwrite(FrameInfo.pFrameBuffer, FrameInfo.nFrameWidth * FrameInfo.nFrameHeight, 1, fp);
					fclose(fp);
				}
			}
			else
			{
				printf("Frame get failed.\r\n");
			}
		}
		if(ch[0] == 's')
		{
			bSaveFrame = !bSaveFrame;
			if(bSaveFrame)
				printf("Now saving frames.\r\n");
			else
				printf("Now not saving frames.\r\n");
		}
		if(ch[0] == 'p')
		{
			if(ch[1] == ' ')
			{
				printf("Input PLL_ID, PLL_CH, PLL_Phase: ");
				int pllid, pllch, pllphase;
				scanf("%d%d%d", &pllid, &pllch, &pllphase);
				UINT32 uParam[3];
				uParam[0] = pllid;
				uParam[1] = pllch;
				uParam[2] = pllphase;
				//DEV_SetDeviceParameter(0xFFFF, SET_PLL_CONFIG, uParam);
				ThreadSleep(100);
				for(int i = 0; i < 8; i++)
				{
					uParam[0] = pllid;
					uParam[1] = i;
					//DEV_GetDeviceParameter(0xFFFF, GET_PLL_STATUS_DATA, uParam);
					printf("PLL Status Word %d is %08x.\r\n", i, uParam[0]);
				}
			}
			else if(ch[1] == '4')
			{
				UINT32 uParam1[3] = {4, 0, 0};
				UINT32 uParam2[3] = {4, 1, 0xE4};
				//DEV_SetDeviceParameter(0xFFFF, SET_SENSOR_CTLWORD, uParam1);
				//DEV_SetDeviceParameter(0xFFFF, SET_SENSOR_CTLWORD, uParam2);
			}
			else if(ch[1] == '0')
			{
				UINT32 uParam1[3] = {0, 0, 0};
				UINT32 uParam2[3] = {0, 1, 0};
				//DEV_SetDeviceParameter(0xFFFF, SET_SENSOR_CTLWORD, uParam1);
				//DEV_SetDeviceParameter(0xFFFF, SET_SENSOR_CTLWORD, uParam2);
			}
			else if(ch[1] == 'r')
			{
				for(int i = 0; i < 8; i++)
				{
					printf("PLL Status Word %d is", i);
					for(int j = 0; j < 4; j++)
					{
						uParam[0] = i;
						uParam[1] = j;
						//DEV_GetDeviceParameter(0xFFFF, GET_PLL_STATUS_DATA, uParam);
						printf(" %08x", uParam[0]);
					}
					//	Restore to normal PLL operation (firmware must be cautious!!)
					uParam[0] = i;
					uParam[1] = 0;
					//DEV_GetDeviceParameter(0xFFFF, GET_PLL_STATUS_DATA, uParam);
					printf("\r\n");
				}
				//DEV_GetDeviceParameter(0xFFFF, GET_IIC_FAILURE_COUNT, uParam);
				printf("IIC Configuration Failure Count is %d\r\n", uParam[0]);
				printf("IIC Configuration Failure Address is 0x%08x\r\n", uParam[1]);
			}
			else if(ch[1] == 'm')
			{
				g_bPausePLLThread = !g_bPausePLLThread;
			}
			else if(ch[1] == 'w')
			{
				//UINT32 uDCM1[2] = {4, 0};
				//for(int i = 0; i < 512; i++)
				//	DEV_SetDeviceParameter(0xFFFF, SET_DCM_INCDEC, uDCM1);
				//UINT32 uDCM2[2] = {4, 1};
				//for(int i = 0; i < 128; i++)
				//	DEV_SetDeviceParameter(0xFFFF, SET_DCM_INCDEC, uDCM2);
			}
		}
		if(ch[0] == 'q')
			bThreadPause = !bThreadPause;
		if(ch[0] == 'e')
			ActivateEvent(pevent);
		if(ch[0] == 'c')
			break;
		if(ch[0] == 'o')
		{
			if(ch[1] == 0)
				ch[1] = '1';
			if(ch[1] >= '0' && ch[1] <= '3')
			{
				uOpenBehavior = ch[1] - '0';
				//DEV_SetDeviceParameter(0xFFFF, SET_OPEN_BEHAVIOR, &uOpenBehavior);
			}
			printf("Begin open device of behavior %d.\r\n", uOpenBehavior);
			DEV_OpenDevice();
			printf("Device opened with behavior %d.\r\n", uOpenBehavior);
			UINT32 nDevList = DEV_GetDeviceList();
			for(int i = 0; i < 32; i++)
			{
				printf("%s", (nDevList & 0x80000000) ? "1" : "0");
				nDevList = nDevList << 1;
			}
			printf("\n");
			//	Read 4 registers
			for(int i = 0; i < 4; i++)
			{
				UINT32 nRegVal = i;
				//DEV_GetDeviceParameter(0xFFFF, GET_SENSOR_REG, &nRegVal);
				printf("Sensor register %d value is %8x\r\n", i, nRegVal);
			}
			printf("\n");
		}
		if(ch[0] == 'k')
			DEV_CloseDevice();
		if((ch[0] == 'g') || (ch[0] == 'h') || (ch[0] == 'i') || (ch[0] == 'j'))
		{
			//	Endpoint index. For RGB, endpoint 1 is used. Else endpoint 0 is used.
			int edpindex = 0;
			if(ch[0] == 'h')
				edpindex = 1;
			int vchindex = 2;
			//	TOF and RGB uses vch 2, and IRIS uses vch 4.
			if((ch[0] == 'i') || (ch[0] == 'j'))
				vchindex = 4;
			//	The index of the 4 iris cameras are 2, 0, 1, 3. Thus for 2 iris configuration the iris camera index is 0 or 1.
			int iriscamindex = 0;
			if(ch[0] == 'j')
				iriscamindex = 1;
			//	Request for a frame for vchindex > 2.
			if(vchindex > 2)
			{
				UINT32 uParam[2];
				uParam[0] = iriscamindex;
				uParam[1] = 100000;
				printf("Request for stream index %d virtual channel %d.\r\n", edpindex, vchindex);
				//for(int j = 0; j < 5; j++)
				//DEV_SetDeviceParameter(0xFFFF, SET_FRAME_REQUEST, uParam);
			}
			//	Request for the frame
			//status = DEV_GetFrame(3, edpindex, vchindex, &devStreamType, &FrameInfo);

			if(FrameInfo.pFrameBuffer != NULL)
			{
				printf("Stream index %d virtual channel %d received.\r\n", edpindex, vchindex);
				printf("Frame width %d height %d timestamp %d framerate %f\r\n", FrameInfo.nFrameWidth, FrameInfo.nFrameHeight, FrameInfo.nTimeStamp, 1000.0 / ((double)FrameInfo.nFrameCycle / (double)FrameInfo.nCycleResolution));
			}
			else
			{
				printf("Frame get failed.\r\n");
			}
		}
		if(ch[0] == 'z')
		{
			UINT32 uParam[] = {32, 32, 5, 200, 1600, 1};
			//DEV_SetDeviceParameter(0xFFFF, SET_CROP_WINDOW_EXT, uParam);
		}
		if(ch[0] == 't')
		{
			for(int c = 0; c < 512; c++)
			{
				USBController* pUsb = new USBController();
				delete pUsb;
			}
		}
		if(ch[0] == 'l')
		{
			if(ch[1] == 0)
			{
				//	Open light for iris 1
				//UINT32 uParam = 0;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET_2, &uParam);
				//uParam = 800000;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET2_2, &uParam);
				//uParam = 50;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_PWM2, &uParam);
				//nLightEN = 1 - nLightEN;
				//uParam = (1 << 16) | 1;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_EN, &uParam);
				////	Enable aux PWM at 30.
				//uParam = 50;
				//DEV_SetDeviceParameter(0xFFFF, SET_AUX_LIGHT_PWM, &uParam);
			}
			else if(ch[1] == '3')
			{
				//	New light control.
				//	Open light for iris 1
				//UINT32 uParam = 0;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET_2, &uParam);
				//uParam = 800000;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET2_2, &uParam);
				//nLightEN = 1 - nLightEN;
				//uParam = (1 << 16) | 1;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_EN, &uParam);
				////	Enable PWM sequence
				//UINT32 uPWM[4] = {100, 100, 100, 0};
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_EXT_PWM, uPWM);
			}
		}
		if(ch[0] == 'm')
		{
			if(ch[1] == 0)
			{
				//	Open light for iris 1
				//UINT32 uParam = 0;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET_2, &uParam);
				//uParam = 800000;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET2_2, &uParam);
				//uParam = 100;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_PWM2, &uParam);
				//nLightEN = 1 - nLightEN;
				//uParam = (2 << 16) | 1;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_EN, &uParam);
				////	Enable aux PWM at 30.
				//uParam = 0;
				//DEV_SetDeviceParameter(0xFFFF, SET_AUX_LIGHT_PWM, &uParam);
			}
			else if(ch[1] == 'r')
			{
				//int lv = parse_int(&ch[2]);
				//	Program of light initial values
				//init_motor();
				//	lv is now between 1 and 8.
				//SetMotorDriverCommand(0, 1, lv);
				init_led();
				SetLEDDriverCommand(15, 700, 0, 511, 0, 63, 24, 1, 1);
			}
			else if(ch[1] == 'g')
			{
				//int lv = parse_int(&ch[2]);
				//	Program of light initial values
				//init_motor();
				//	lv is now between 1 and 8.
				//SetMotorDriverCommand(0, 2, lv);
				init_led();
				SetLEDDriverCommand(15, 700, 0, 511, 64, 63, 24, 1, 1);
			}
			else if(ch[1] == 'b')
			{
				//int lv = parse_int(&ch[2]);
				//	Program of light initial values
				//init_motor();
				//	lv is now between 1 and 8.
				//SetMotorDriverCommand(0, 3, lv);
				init_led();
				SetLEDDriverCommand(15, 700, 0, 511, 128, 63, 24, 1, 1);
			}
			else if(ch[1] == 'w')
			{
				//int lv = parse_int(&ch[2]);
				//	Program of light initial values
				//init_motor();
				//	lv is now between 0 and 7.
				//SetMotorDriverCommand(0, 7, lv);
				init_led();
				SetLEDDriverCommand(15, 700, 0, 511, 384, 63, 24, 1, 1);
			}
			else if(ch[1] == 'i')
			{
				nLEDbarinvert = nLEDbarinvert ^ 0x0F;
				InvertLEDDriverOutput(nLEDbarinvert);
			}
			else if(ch[1] == '3')
			{
				//	New light control.
				//	Open light for iris 1
				//UINT32 uParam = 0;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET_2, &uParam);
				//uParam = 800000;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_OFFSET2_2, &uParam);
				//nLightEN = 1 - nLightEN;
				//uParam = (1 << 16) | 1;
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_EN, &uParam);
				////	Enable PWM sequence
				//UINT32 uPWM[4] = {0, 100, 100, 100};
				//DEV_SetDeviceParameter(0xFFFF, SET_LIGHT_EXT_PWM, uPWM);
			}
		}
		if(ch[0] == 'u')
		{
			UINT32 uParam = 0;
			if(ch[1] == '1')
				uParam = 1;
			//DEV_SetDeviceParameter(0xFFFF, SET_USBDOG_RESET, &uParam);
		}
	}

	bThreadClose = true;
	for(int i = 0; i <= thread_cnt; i++)
	{
		if(threads[i] != NULL)
		{
			WaitForThread(threads[i], 0xFFFFFFFF);
		}
	}
	for(int i = 0; i <= thread_cnt; i++)
	{
		if(threads[i] != NULL)
			CloseThread(threads[i]);
	}

	delete []threads;

	delete []prgbabuf;

	DeleteSemaphore(pmutex);
	DeleteEvent(pevent);
	DEV_CloseDevice();
	printf("Test done.\r\n");
	return 0;
}
