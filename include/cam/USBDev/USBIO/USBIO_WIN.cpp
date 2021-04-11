#include "USBIO.h"
#include "../USBDev.h"
#include "../DEVDEF.h"

#include <Windows.h>
#include "CyAPI.h"

static int gnDeviceCount = 0;
static int gDeviceIndex[MAX_DEV_CNT] = {0};


int GetDeviceCount(int* ven_id, int* dev_id, int numids)
{
	//	Clear buffer
	gnDeviceCount = 0;
	memset(gDeviceIndex, 0, MAX_DEV_CNT * sizeof(int));

	//	Create a device instance
	CCyUSBDevice* pDevice = new CCyUSBDevice();
	if (pDevice == NULL)
		return 0;

	// Get USB device count
	gnDeviceCount = 0;
	int nDeviceCnt =  pDevice->DeviceCount();
	for(int i = 0; i < nDeviceCnt; i++)
	{
		pDevice->Open(i);
		//	Verify VENDOR_ID and DEVICE_ID
		for(int j = 0; j < numids; j++)
		{
			if((pDevice->VendorID == ven_id[j]) && (pDevice->ProductID == dev_id[j]))
			{
				gDeviceIndex[gnDeviceCount++] = i;
				break;
			}
		}
		pDevice->Close();
	}
	delete pDevice;
	return gnDeviceCount;
}

void* CreateDeviceHandler(int index)
{
	//	Call GetDeviceCount if zero; create CCyUSBDevice instance.
	if(gnDeviceCount == 0)
	{
		GetDeviceCount((int*)g_nCustomFx3VenID, (int*)g_nCustomFx3DevID, g_nCustomFx3DevIDCnt);
	}
	if(index >= gnDeviceCount)
		return NULL;
	
	//	Open specific device
	CCyUSBDevice* pDevice = new CCyUSBDevice();
	if (pDevice == NULL)
		return NULL;
	bool bRet = pDevice->Open(gDeviceIndex[index]);
	//	On success return device handler; else return nothing.
	if(bRet)
		return pDevice;
	delete pDevice;
	return NULL;
}

void CloseDeviceHandler(void* devhandle)
{
	CCyUSBDevice* pDevice = (CCyUSBDevice*)devhandle;
	if(pDevice == NULL)
		return;
	pDevice->Close();
	delete pDevice;
}

void CloseDevices()
{
	//	Do nothing in windows
}

void* GetEndpoint(void* devhandle, int epnum)
{
	CCyUSBDevice* pDevice = (CCyUSBDevice*)devhandle;
	if(pDevice == NULL)
		return NULL;
	int nEPCount = pDevice->EndPointCount();
	for(int i = 0; i < nEPCount; i++)
	{
		if(pDevice->EndPoints[i]->Address == epnum)
			return pDevice->EndPoints[i];
	}
	//	If not found, return NULL.
	return NULL;
}

bool TransferOutEndpoint(void* epout, BYTE* buffer, int* plen, int timeout)
{
	LONG ltransfer = *plen;
	CCyUSBEndPoint* pEP = (CCyUSBEndPoint*)epout;
	if(pEP == NULL)
	{
		*plen = 0;
		return false;
	}
	pEP->TimeOut = timeout;
	//pEP->SetXferSize(*plen);
	bool bRet = pEP->XferData(buffer, ltransfer);
	*plen = ltransfer;
	return bRet;
}

bool TransferInEndpoint(void* epin, BYTE* buffer, int* plen, int timeout)
{
	LONG ltransfer = *plen;
	CCyUSBEndPoint* pEP = (CCyUSBEndPoint*)epin;
	if(pEP == NULL)
	{
		*plen = 0;
		return false;
	}
	pEP->TimeOut = timeout;
	//pEP->SetXferSize(*plen);
	bool bRet = pEP->XferData(buffer, ltransfer);
	*plen = ltransfer;
	return bRet;
}

int ResetEndpoint(void* ep)
{
	CCyUSBEndPoint* pEP = (CCyUSBEndPoint*)ep;
	// return pEP->Abort();
	return pEP->Reset();
}

void DownloadFirmware(void* devhandle, char* pfile)
{
	CCyFX3Device* pDevice = (CCyFX3Device*)devhandle;
	pDevice->DownloadFw(pfile, RAM);
}

void DownloadFirmware(void* devhandle, char* pdata, int datalen)
{
	CCyFX3Device* pDevice = (CCyFX3Device*)devhandle;
	pDevice->DownloadFw(pdata, datalen, RAM);
}

int GetEndpointBurstLength(void* epout)
{
	CCyUSBEndPoint* pEP = (CCyUSBEndPoint*)epout;
	if(pEP == NULL)
	{
		return 0;
	}
	return pEP->MaxPktSize;
}

bool CheckDeviceOnline(void* devhandle)
{
	CCyFX3Device* pDevice = (CCyFX3Device*)devhandle;
	return true;
}