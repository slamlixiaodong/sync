
#include "USBIO/USBIO.h"


#include "USBController.h"

#include "Bayer2RGBA.h"


extern volatile int gbPrintDebugInfo;

extern volatile int TOF_TRIGGER_DELAY_TIME;


UINT32 DwordToBe(UINT32 u32)
{
	return (u32 << 24) | ((u32 & 0xFF00) << 8) | ((u32 & 0xFF0000) >> 8) | (u32 >> 24);
}

UINT32 UShortToBe(UINT16 u16)
{
	return (UINT32)((u16 >> 8) | (u16 << 8));
}


void DumpArray(char* pfile, UINT32* parr, int len)
{
	FILE* fp = fopen(pfile, "wb");
	if(fp != NULL)
	{
		fwrite(parr, 4, len, fp);
		fclose(fp);
	}
}

USBController::USBController()
{
	m_bCloseDevice = FALSE;
	m_pUSBDevice = NULL;
	m_pEPCmdOut = NULL;
	m_pEPRespIn = NULL;
	m_nStreamCount = 1;
	m_pFrameRecvParam = NULL;
	m_bDropBadFrame = false;
	//m_pLogFile = NULL;
	//m_pLogFileLock = CreateLock();
	//_Debug_Log = true;
	m_dTransferDataRate = 0;
	m_dTransferFrameRate = 0;
	m_nWidthDivide = 1;
	m_nHeightMultiply = 1;
	m_bPauseRecvStream = false;
	m_nSensorFrameCyc = 0;
	m_bRecordThreadSt = FALSE;
	if(RECEIVE_DUMP_DATA)
		m_pRawRecordFile = fopen("Record.bin", "wb");
	else
		m_pRawRecordFile = NULL;
	m_bCropBaseOrgEn = false;
	m_nCropBaseXOrg = 0;
	m_nCropBaseYOrg = 0;
	m_nSPI_FlashSize = 0;
	m_nSPI_FPGABaseAddr = 0;
	m_nSPI_FWBaseAddr = 0;
	m_nSPI_ConfigBaseAddr = 0;
	m_nSPI_SignatureBaseAddr = 0;
	m_nSPI_DevIndex = 0;
	//m_nMagicDataBuf = new UINT32[16384];

	m_bDebugBreak = false;

	m_thEPBulkOut = NULL;

	memset(m_nCropBaseXOrg_Total, 0, 16 * sizeof(int));
	memset(m_nCropBaseYOrg_Total, 0, 16 * sizeof(int));
	memset(m_nCropBaseWidth_Total, 0, 16 * sizeof(int));
	memset(m_nCropBaseHeight_Total, 0, 16 * sizeof(int));

	for(int i = 0; i < MAX_STREAM_CNT * MAX_VIRTCH_CNT; i++)
	{
		m_nStreamLostConfirm[i] = 0;
	}

	m_nCmdRespPacketSize = 512;

	//	AGC & AEC is enabled on start up
	m_b034AGCEnable = 1;
	m_b034AECEnable = 1;
	m_n034BinMode = 0;
	m_bDeviceOpened = 0;
}

USBController::~USBController()
{
	CloseDevice();
	if(RECEIVE_DUMP_DATA)
		fclose(m_pRawRecordFile);
	if(m_pFrameRecvParam)
		delete []m_pFrameRecvParam;
	m_pFrameRecvParam = NULL;
	if(m_pUSBDevice)
	{
		CloseDeviceHandler(m_pUSBDevice);
		delete m_pUSBDevice;
	}
}

UINT32 nqueen(int queen)
{
	UINT32 qt[1024];
	UINT32 len = 0;

	UINT32 result = 0;

	UINT32 mask = 0;
	UINT32 i;

	qt[len + 0] = 0;
	qt[len + 1] = 0;
	qt[len + 2] = 0;
	len += 3;

	for(i = 0; i < queen; i++)
		mask |= (1 << i);

	while(len > 0)
	{
		UINT32 t_rd, t_ld, t_q, t_used, t_unused;

		t_rd = qt[len - 1];
		t_ld = qt[len - 2];
		t_q = qt[len - 3];
		len -= 3;
		t_used = mask & (t_q | t_ld | t_rd);

		if(t_used == mask)
			continue;

		t_unused = mask & (~t_used);

		while(t_unused)
		{
			UINT32 l_bit = 0;
			UINT32 l_q;
			UINT32 l_ld;
			UINT32 l_rd;

			/*	Find an empty cell */
			l_bit = t_unused & -t_unused;
			t_unused ^= l_bit;

			l_q = t_q | l_bit;
			l_ld = (t_ld | l_bit) << 1; 
			l_rd = (t_rd | l_bit) >> 1; 
			if(l_q == mask)
				result++;
			else
			{
				qt[len + 0] = l_q;
				qt[len + 1] = l_ld;
				qt[len + 2] = l_rd;
				len += 3;
			}
		}

	}
	return result;
}

DEV_STATUS USBController::DEV_WriteCmdEndpoint(UINT32* pData, int timeout)
{
	int pktlen = m_nCmdRespPacketSize;
	if(false == TransferOutEndpoint(m_pEPCmdOut, (BYTE*)pData, &pktlen, timeout))
		return DEV_FAIL;
	else
		return DEV_SUCCESS;
}

DEV_STATUS USBController::DEV_ReadRespEndpoint(UINT32* pData, int timeout)
{
	int pktlen = m_nCmdRespPacketSize;
	if(false == TransferInEndpoint(m_pEPRespIn, (BYTE*)pData, &pktlen, timeout))
		return DEV_FAIL;
	else
		return DEV_SUCCESS;
}

DEV_STATUS USBController::OpenDevice(int device_index, int init)
{
	if(m_pUSBDevice)
		return DEV_ALREADYOPENED;
	m_nDeviceIndex = device_index;
	m_nStreamCount = 1;
	DEV_STATUS bStatus = DEV_SUCCESS;
	//	Create and open device. If failed, the result is NULL.
	m_pUSBDevice = CreateDeviceHandler(device_index);
	if(m_pUSBDevice == NULL)
		return DEV_DEVICENOTFOUND;

	UINT32 pBulk[256];
	int bulklen = 1024;

	// Update end points
	m_pEPCmdOut = GetEndpoint(m_pUSBDevice, END_POINT_CMD_OUT);
	m_pEPRespIn = GetEndpoint(m_pUSBDevice, END_POINT_RESP_IN);
	m_pEPStreamIn = GetEndpoint(m_pUSBDevice, END_POINT_STREAM_IN);
	//	Check for basic endpoint configuration.
	if(m_pEPCmdOut == NULL || m_pEPRespIn == NULL || m_pEPStreamIn == NULL)
	{
		if(gbPrintDebugInfo >= LOG_CRITICAL)
		{
			char pLogBuf[1024];
			sprintf(pLogBuf, "File USBController.cpp Line %5d: Requested endpoints do not exist.", __LINE__);
			printf("%s\r\n", pLogBuf);
			DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
		}
		CloseDeviceHandler(m_pUSBDevice);
		m_pUSBDevice = NULL;
		return DEV_USBFWNOTREADY;
	}

	ResetEndpoint(m_pEPCmdOut);
	ResetEndpoint(m_pEPRespIn);
	if(m_pEPStreamIn != NULL)
		ResetEndpoint(m_pEPStreamIn);


	m_bCloseDevice = FALSE;
	m_bPauseRecvStream = false;

	if(gbPrintDebugInfo >= LOG_REQUIRED)
	{
		char pLogBuf[1024];
		sprintf(pLogBuf, "File USBController.cpp Line %5d: Trying to enter rescue mode.", __LINE__);
		printf("%s\r\n", pLogBuf);
		DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
	}


	//	Firstly check for hardware operationality. If any bit error is detected, report error.
	if(gbPrintDebugInfo >= LOG_REQUIRED)
	{
		char pLogBuf[1024];
		sprintf(pLogBuf, "File USBController.cpp Line %5d: Begin check for device hardware operationality.", __LINE__);
		printf("%s\r\n", pLogBuf);
		DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
	}
	
	bStatus = VerifyDeviceHWStatus();
	if(bStatus != DEV_SUCCESS)
		return bStatus;

	if(gbPrintDebugInfo >= LOG_CRITICAL)
	{
		char pLogBuf[1024];
		sprintf(pLogBuf, "File USBController.cpp Line %5d: Device hardware verified.", __LINE__);
		printf("%s\r\n", pLogBuf);
		DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
	}

	if(gbPrintDebugInfo >= LOG_NORMAL)
	{
		char pLogBuf[1024];
		sprintf(pLogBuf, "File USBController.cpp Line %5d: Begin entering user mode.", __LINE__);
		printf("%s\r\n", pLogBuf);
		DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
	}
	m_nSensorStatus = DEV_SUCCESS;
	//	IIC Test
	BYTE raddr = 0;
	BYTE rdata[256] = {0};
	IICGeneralAccess(0x90, &raddr, rdata, 1, 256, 0, 1);

	m_bDeviceOpened = 1;
	return DEV_SUCCESS;
}

DEV_STATUS USBController::VerifyDeviceHWStatus()
{
	UINT32 pCmd[256] = {0};
	UINT32 pResp[256] = {0};
	int retrycnt = 0;
	pCmd[0] = 0x60606060;
	pCmd[1] = rand();
	pCmd[2] = rand();
	pCmd[3] = rand();
	//	Retry for at most 4 times
	for(retrycnt = 0; retrycnt < 4; retrycnt++)
	{
		if(DEV_SUCCESS == CommitCommand(pCmd, pResp, true, 200))
			return DEV_SUCCESS;
	}
	return DEV_HARDWAREERROR;
}

DEV_STATUS USBController::IICGeneralAccess(BYTE slvaddr, BYTE *pRegAddr, BYTE *pRegData, UINT32 ralen, UINT32 dalen, UINT32 iicsel, UINT32 bR1W0)
{
	BYTE pCmd[1024] = {0};
	BYTE pResp[1024] = {0};
	int i;
	if(ralen > 4)
		ralen = 4;
	if(dalen > 256)
		dalen = 256;
	pCmd[0] = CMD_TYPE_IIC;
	pCmd[4] = bR1W0 ? CMD_IIC_READ : CMD_IIC_WRITE;
	//	[8] IIC_INDEX [9] IIC_SLVADDR [A] IIC_RALEN [B] IIC_DALEN_L [C] IIC_DALEN_H [D] IIC_RADDR [D+M] IIC_DATA (if any)
	pCmd[8] = iicsel;
	pCmd[9] = slvaddr;
	pCmd[10] = ralen;
	pCmd[11] = (dalen & 0xFF);
	pCmd[12] = ((dalen >> 8) & 0xFF);
	for(i = 0; i < ralen; i++)
		pCmd[13 + i] = pRegAddr[i];
	if(bR1W0 == 0)
	{
		//	Write data
		for(i = 0; i < dalen; i++)
			pCmd[13 + ralen + i] = pRegData[i];
	}
	//	Send command and receive response
	DEV_STATUS stat = CommitCommand((UINT32*)pCmd, (UINT32*)pResp, true, 200);
	if(stat == DEV_SUCCESS)
	{
		if(bR1W0)
		{
			for(i = 0; i < dalen; i++)
				pRegData[i] = pResp[16 + i];
		}
		if(pResp[16 + (bR1W0 ? dalen : 0)] == 1)
			return DEV_NOACK;
		return DEV_SUCCESS;
	}
	return DEV_FAIL;
}

DEV_STATUS USBController::WriteRegister(BYTE slvaddr, UINT32 nRegAddr, UINT32 nRegData, UINT32 ralen, UINT32 dalen, UINT32 iicsel)
{
	BYTE regaddr[4];
	BYTE regdata[4];
	int i;
	if(iicsel >= 4)
		return DEV_OUTOFINDEX;
	if(ralen > 4)
		ralen = 4;
	if(dalen > 4)
		dalen = 4;
	for(i = ralen - 1; i >= 0; i--)
	{
		regaddr[i] = (BYTE)nRegAddr;
		nRegAddr = nRegAddr >> 8;
	}
	for(i = dalen - 1; i >= 0; i--)
	{
		regdata[i] = (BYTE)nRegData;
		nRegData = nRegData >> 8;
	}
	return IICGeneralAccess(slvaddr, regaddr, regdata, ralen, dalen, iicsel, 0);
}

DEV_STATUS USBController::ReadRegister(BYTE slvaddr, UINT32 nRegAddr, UINT32 ralen, UINT32 dalen, UINT32* rdto, UINT32 iicsel)
{
	BYTE regaddr[4];
	BYTE regdata[4];
	UINT32 urd = 0;
	DEV_STATUS status;
	int i;
	if(iicsel >= 4)
		return DEV_OUTOFINDEX;
	if(ralen > 4)
		ralen = 4;
	if(dalen > 4)
		dalen = 4;
	for(i = ralen - 1; i >= 0; i--)
	{
		regaddr[i] = (BYTE)nRegAddr;
		nRegAddr = nRegAddr >> 8;
	}
	status = IICGeneralAccess(slvaddr, regaddr, regdata, ralen, dalen, iicsel, 1);
	for(i = 0; i < dalen; i++)
	{
		urd = urd << 8;
		urd |= (((UINT32)regdata[i]) & 0x000000FF);
	}
	*rdto = urd;
	return status;
}

DEV_STATUS USBController::Set034Exposure(UINT32 bEnAEC, UINT32 bEnHDR, UINT32 bEnAutoAdjust, UINT32 coarseint, UINT32 knee1, UINT32 knee2, UINT32 minint, UINT32 maxint)
{
	m_b034AECEnable = bEnAEC ? 1 : 0;
	//	Write AEC enable first
	WriteRegister(0x90, 0xAF, ((m_b034AGCEnable << 1) | m_b034AECEnable), 1, 2, 0);
	//	Write HDR
	WriteRegister(0x90, 0x0F, bEnHDR ? 0x0001 : 0x0000, 1, 2, 0);
	//	Write Auto Adjust
	WriteRegister(0x90, 0x0A, bEnAutoAdjust ? 0x0164 : 0x0064, 1, 2, 0);
	//	Write Min / Max integration
	WriteRegister(0x90, 0xAC, minint, 1, 2, 0);
	WriteRegister(0x90, 0xAD, maxint, 1, 2, 0);
	//	Write integration
	WriteRegister(0x90, 0x0B, coarseint, 1, 2, 0);
	WriteRegister(0x90, 0x08, knee1, 1, 2, 0);
	return WriteRegister(0x90, 0x09, knee2, 1, 2, 0);
}

DEV_STATUS USBController::Set034AnalogGain(UINT32 bEnAGC, UINT32 gain, UINT32 maxgain)
{
	m_b034AGCEnable = bEnAGC ? 1 : 0;
	//	Write AEC enable first
	WriteRegister(0x90, 0xAF, ((m_b034AGCEnable << 1) | m_b034AECEnable), 1, 2, 0);
	//	Write gain value
	WriteRegister(0x90, 0x35, gain, 1, 2, 0);
	//	Write max gain value
	return WriteRegister(0x90, 0xAB, maxgain, 1, 2, 0);
}

DEV_STATUS USBController::Set034DesiredBin(UINT32 nDesired)
{
	return WriteRegister(0x90, 0xA5, nDesired, 1, 2, 0);
}

DEV_STATUS USBController::Set034Reset(UINT32 reset)
{
	return WriteRegister(0x90, 0x0C, reset ? 0x03 : 0x00, 1, 2, 0);
}

DEV_STATUS USBController::Set034BinMode(UINT32 bin)
{
	if(bin > 2)
		bin = 0;
	this->m_n034BinMode = bin;
	//	Write BIN mode
	return WriteRegister(0x90, 0x0D, m_n034BinMode, 1, 2, 0);
}

DEV_STATUS USBController::Get034KneeAutoAdjust(UINT32* pData)
{
	DEV_STATUS status = ReadRegister(0x90, 0x0A, 1, 2, pData, 0);
	pData[0] = (pData[0] >> 8) & 0x01;
	return status;
}

DEV_STATUS USBController::Set034Initialize()
{
	WriteRegister(0x90, 0xB1, 0x0000, 1, 2, 0);
	WriteRegister(0x90, 0xB2, 0x0000, 1, 2, 0);
	WriteRegister(0x90, 0xB6, 0x0001, 1, 2, 0);
	return WriteRegister(0x90, 0x07, 0x0118, 1, 2, 0);
}

DEV_STATUS USBController::Get034HDRState(UINT32* pData)
{
	return ReadRegister(0x90, 0x0F, 1, 2, pData, 0);
}

DEV_STATUS USBController::Get034AGCMaxGain(UINT32* pData)
{
	return ReadRegister(0x90, 0xAB, 1, 2, pData, 0);
}

DEV_STATUS USBController::Get034DesiredBin(UINT32* pData)
{
	return ReadRegister(0x90, 0xA5, 1, 2, pData, 0);
}

DEV_STATUS USBController::Get034AnalogGain(UINT32* pData)
{
	return ReadRegister(0x90, 0x35, 1, 2, pData, 0);
}

DEV_STATUS USBController::Get034KneePoints(UINT32* pData)
{
	ReadRegister(0x90, 0x08, 1, 2, pData + 0, 0);
	return ReadRegister(0x90, 0x09, 1, 2, pData + 1, 0);
}

DEV_STATUS USBController::Get034AECAGCStatus(UINT32* pData)
{
	return ReadRegister(0x90, 0xAF, 1, 2, pData, 0);
}

DEV_STATUS USBController::Get034TotalInteg(UINT32* pData)
{
	return ReadRegister(0x90, 0x0B, 1, 2, pData, 0);
}

DEV_STATUS USBController::Get034AECMinMaxCoarseInteg(UINT32* pData)
{
	ReadRegister(0x90, 0xAC, 1, 2, pData + 0, 0);
	return ReadRegister(0x90, 0xAD, 1, 2, pData + 1, 0);
}

DEV_STATUS USBController::SendCommandPacket(int nIndex, int nCmd, UINT32 nData, UINT32 *pResp, bool bVerify, int timeout)
{
	BYTE pCmd[1024] = {0};
	pCmd[0] = CMD_TYPE_SENSOR;
	pCmd[4] = CMD_TRIGGER_WRITE;
	//	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
	pCmd[8] = (BYTE)nIndex;					//	Module Device Index
	pCmd[9] = (BYTE)nCmd;
	pCmd[10] = (nData >> 24) & 0xFF;
	pCmd[11] = (nData >> 16) & 0xFF;
	pCmd[12] = (nData >> 8) & 0xFF;
	pCmd[13] = (nData >> 0) & 0xFF;
	pCmd[14] = ~(pCmd[8] + pCmd[9] + pCmd[10] + pCmd[11] + pCmd[12] + pCmd[13]);
	return CommitCommand((UINT32*)pCmd, (UINT32*)pResp, true, 200);
}

DEV_STATUS USBController::SetTriggerMode(UINT32 dwMode)
{
	//BYTE pCmd[1024] = {0};
	BYTE pResp[1024] = {0};
	if(dwMode == 0)
	{
		BYTE regaddr = 0x07;
		BYTE regdata[2] = {0};
		IICGeneralAccess(0x90, &regaddr, regdata, 1, 2, 0, 1);
		regdata[0] = 0x01;
		regdata[1] = 0x08;
		IICGeneralAccess(0x90, &regaddr, regdata, 1, 2, 0, 0);
	}
	else
	{
		BYTE regaddr = 0x07;
		BYTE regdata[2] = {0};
		IICGeneralAccess(0x90, &regaddr, regdata, 1, 2, 0, 1);
		regdata[0] = 0x01;
		regdata[1] = 0x18;
		IICGeneralAccess(0x90, &regaddr, regdata, 1, 2, 0, 0);
		//	When use internal trigger, set mode to 0. Else set mode to 1
		return SendCommandPacket(0x01, 0x11, (dwMode == 1) ? 0x00 : 0x01, (UINT32*)pResp, true, 200);
		////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
	}
	return DEV_SUCCESS;
}

DEV_STATUS USBController::SetPWMMode(UINT32 index, UINT32 period, UINT32 duty, UINT32 enable)
{
	BYTE pResp[1024] = {0};
	if(DEV_SUCCESS != SendCommandPacket(index + 0x03, 0x00, period, (UINT32*)pResp, true, 200))
		return DEV_FAIL;
	////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
	//	Select duty cycle
	if(DEV_SUCCESS != SendCommandPacket(index + 0x03, 0x01, duty, (UINT32*)pResp, true, 200))
		return DEV_FAIL;
	//	Set enable & update
	if(DEV_SUCCESS != SendCommandPacket(index + 0x03, 0x02, (enable << 31) | 0x01, (UINT32*)pResp, true, 200))
		return DEV_FAIL;
	return DEV_SUCCESS;
}

DEV_STATUS USBController::SetTriggerInterval(UINT32 dwInterval)
{
	BYTE pResp[1024] = {0};
	return SendCommandPacket(0x01, 0x13, dwInterval, (UINT32*)pResp, true, 200);
	//	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
}

DEV_STATUS USBController::SetTriggerOnce(UINT32 dwTrig)
{
	BYTE pResp[1024] = {0};
	WriteRegister(0x90, 0x0C, 0x0003, 1, 2, 0);
	return SendCommandPacket(0x01, 0x14, dwTrig, (UINT32*)pResp, true, 200);
	////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
}

DEV_STATUS USBController::SetTriggerPinMode(UINT32 dwMode)
{
	BYTE pResp[1024] = {0};
	return SendCommandPacket(0x01, 0x12, dwMode, (UINT32*)pResp, true, 200);
	////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
}

DEV_STATUS USBController::Set034LEDMode(UINT32 dwMode)
{
	BYTE pResp[1024] = {0};
	return SendCommandPacket(0x01, 0x01, dwMode, (UINT32*)pResp, true, 200);
	////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
}

DEV_STATUS USBController::Set034LEDPolary(UINT32 dwPol)
{
	BYTE pResp[1024] = {0};
	return SendCommandPacket(0x01, 0x02, dwPol, (UINT32*)pResp, true, 200);
	////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
}

DEV_STATUS USBController::Set034LEDOnTime(UINT32 dwTime)
{
	BYTE pResp[1024] = {0};
	return SendCommandPacket(0x01, 0x03, dwTime, (UINT32*)pResp, true, 200);
	////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
}

DEV_STATUS USBController::Set034ResetPin(UINT32 bReset)
{
	BYTE pResp[1024] = {0};
	return SendCommandPacket(0x02, 0x01, bReset ? 0 : 1, (UINT32*)pResp, true, 200);
	////	[8] MODULEINDEX [9] CMD [10] BYTE3 [11] BYTE2 [12] BYTE1 [13] BYTE0 [14] CRC
}

DEV_STATUS USBController::CommitCommand(UINT32* pCmd, UINT32* pResp, bool bVerify, int timeout)
{
	DEV_STATUS status = DEV_WriteCmdEndpoint(pCmd, timeout);
	bool bMatch = false;

	if(status != DEV_SUCCESS)
	{
		//	Retry once
		ResetEndpoint(m_pEPCmdOut);
		status = DEV_WriteCmdEndpoint(pCmd, timeout);
		if(status != DEV_SUCCESS)
			return DEV_FAIL;
	}
	while(bVerify)
	{
		//	Try read until match. Retry until fail.
		status = DEV_ReadRespEndpoint(pResp, timeout);
		if(status != DEV_SUCCESS)
		{
			ResetEndpoint(m_pEPRespIn);
			status = DEV_ReadRespEndpoint(pResp, timeout);
			if(status != DEV_SUCCESS)
				return DEV_FAIL;
		}

		//	Verify header. If match, break.
		bMatch = true;
		for(int i = 0; i < 4; i++)
		{
			if(pResp[i] != pCmd[i])
			{
				bMatch = false;
				break;
			}
		}
		if(bMatch)
			break;
	}
	if(bVerify)
	{
		if(bMatch)
			return DEV_SUCCESS;
		return DEV_FAIL;
	}
	return DEV_SUCCESS;
}

DEV_STATUS USBController::GetDeviceID(UINT32* pid)
{
	UINT32 pBulk[256];
	int bulklen = 1024;
	//	Enter rescue mode and read DNA
	pBulk[0] = 0x0A;
	bulklen = 1024;
	TransferOutEndpoint(m_pEPCmdOut, (PUCHAR)pBulk, &bulklen, 200);
	TransferInEndpoint(m_pEPRespIn, (PUCHAR)pBulk, &bulklen, 200);
	pid[0] = pBulk[0];
	pid[1] = pBulk[1];
	return DEV_SUCCESS;
}

DEV_STATUS USBController::InitDevice()
{
	UINT32 pBulk[256];
	int bulklen = 1024;

	m_bMultiThreaded = false;
	m_bCropBaseOrgEn = false;
	m_nStreamCount = 1;
	USHORT uRegVal = 0;
	UINT32 uRegValD = 0;

	UINT32 uData[256] = {0};

	if(m_pFrameRecvParam)
		delete []m_pFrameRecvParam;

	m_pFrameRecvParam = new FrameRecvParam[m_nStreamCount];
	for(int i = 0; i < m_nStreamCount; i++)
	{
		m_pFrameRecvParam[i].m_pFirstVirtualCh->m_nStreamIndex = i;
	}
	m_pFrameRecvParam[0].m_pFirstVirtualCh->m_pEPStreamIn = m_pEPStreamIn;

	this->Set034ResetPin(0);

	WriteRegister(0x90, 0x07, 0x0108, 1, 2, 0);
	WriteRegister(0x90, 0x20, 0x03C7, 1, 2, 0);
	WriteRegister(0x90, 0x24, 0x001B, 1, 2, 0);
	WriteRegister(0x90, 0x2B, 0x0003, 1, 2, 0);
	WriteRegister(0x90, 0x2F, 0x0003, 1, 2, 0);

	this->Set034Initialize();

	m_bDeviceOpened = 1;
	return DEV_SUCCESS;
}

int USBController::GetUSBDeviceCount()
{
	return GetDeviceCount((int*)g_nCustomFx3VenID, (int*)g_nCustomFx3DevID, g_nCustomFx3DevIDCnt);
}

DEV_STATUS USBController::ValidateHardware(UINT32* pi)
{
	UINT32 ptn[256] = {0};
	int i = 0;
	for(; i < 32; i++)
		ptn[i] = (0x00000001 << i);
	for(i = 0; i < 32; i++)
	{
		if(ptn[i] != pi[i])
		{
			if(gbPrintDebugInfo >= LOG_CRITICAL)
			{
				char pLogBuf[1024];
				sprintf(pLogBuf, "File USBController.cpp Verification failed. %d Expected: 0x%8x, Actual: 0x%8x\r\n", i, ptn[i], pi[i]);
				printf("%s\r\n", pLogBuf);
				DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
			}
			return DEV_HARDWAREERROR;
		}
	}
	return DEV_SUCCESS;
}

DEV_STATUS USBController::CloseDevice()
{
	//NOSE25K_PowerUp(0);
	//m_AD9928.AD9928Shutdown();
	//ResetSensor();
	//	Shut down sensors
	if(m_bDeviceOpened)
		this->Set034ResetPin(1);
	m_bDeviceOpened = 0;
	m_bCloseDevice = TRUE;
	while(m_bRecordThreadSt)
		ThreadSleep(50);
	if(m_thEPBulkOut)
	{
		WaitForThread(m_thEPBulkOut, 0xFFFFFFFF);
		CloseThread(m_thEPBulkOut);
		m_thEPBulkOut = NULL;
	}
	if(m_pFrameRecvParam)
		delete []m_pFrameRecvParam;
	m_pFrameRecvParam = NULL;
	if(m_pUSBDevice)
	{
		CloseDeviceHandler(m_pUSBDevice);
		m_pUSBDevice = NULL;
	}
	return DEV_SUCCESS;
}

int GetSPIFlashSectorSize(int jedec)
{
	//	Parse JEDEC ID
	int mft_id = (int)(jedec & 0x00FF);
	int mem_type = (int)((jedec >> 8) & 0x00FF);
	int mem_sz = (int)((jedec >> 16) & 0x00FF);
	if (mft_id == 0x20)
	{
		//	Numonyx SPI Flash
		//	0x20 Erase Sector command is not supported.
		if (mem_type == 0x20)
		{
			//	SPI Flash
			if (mem_sz == 0x18)
			{
				//	M25P128. Block size is 256KB
				return 262144;
			}
			else if (mem_sz == 0x17)
			{
				//	M25P64. Block size is 64KB
				return 65536;
			}
			else if (mem_sz == 0x14)
			{
				//	M25P80. Block size is 64KB
				return 65536;
			}
			else if (mem_sz == 0x13)
			{
				//	M25P40. Block size is 64KB
				return 65536;
			}
			else if (mem_sz == 0x12)
			{
				//	M25P20. Block size is 64KB
				return 65536;
			}
		}
	}
	else if (mft_id == 0xEF)
	{
		//	Winbond SPI Flash
		//	0x20 Erase Sector command is supported.
		if (mem_type == 0x40 || mem_type == 0x60)
		{
			//	SPI Flash
			if (mem_sz == 0x18)
			{
				//	W25Q128. Block size is 64KB.
				return 65536;
			}
			else if (mem_sz == 0x17)
			{
				//	W25Q64. Block size is 64KB.
				return 65536;
			}
			else if (mem_sz == 0x16)
			{
				//	W25Q32. Block size is 64KB.
				return 65536;
			}
		}
	}
	return 65536;
}

DEV_STATUS USBController::DropRespInPackets()
{
	//	If there're any packets, drop them.
	UINT32 pBulk[256];
	UINT32 pResp[256];
	int bulklen = 1024;
	bool bRet;
	DEV_STATUS status;

	if(gbPrintDebugInfo >= LOG_UNIMPORTANT)
	{
		char pLogBuf[1024];
		sprintf(pLogBuf, "File USBController.cpp Line %5d: Begin dropping response in endpoint packets.", __LINE__);
		printf("%s\r\n", pLogBuf);
		DEV_WriteLogFile(1, this->m_nDeviceIndex, (BYTE*)pLogBuf);
	}

	//	Try read some bits
	pBulk[0] = 0x60606060;
	pBulk[1] = rand();
	pBulk[2] = rand();
	pBulk[3] = rand();
	for(int i = 0; i < 4; i++)
	{
		status = this->CommitCommand(pBulk, pResp, true, 200);
	}

	//	Try read DNA
	pBulk[0] = 0x66666666;
	pBulk[1] = 0x00000001;
	pBulk[2] = rand();
	pBulk[3] = rand();
	status = this->CommitCommand(pBulk, pResp, true, 200);

	return status;
}
