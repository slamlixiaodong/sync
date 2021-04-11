#include "USBIO.h"
#include "../USBDev.h"
#include "../DEVDEF.h"

#include "cyusb.h"

#include "../Misc/USBDev_Threads.h"

#include <string.h>
#include <vector>
#include <stdio.h>

#include <libusb-1.0/libusb.h>

static volatile int gnDeviceCount = 0;
static volatile int gnDeviceOpened[MAX_DEV_CNT] = {0};
static volatile int gnDeviceIndex[MAX_DEV_CNT] = {0};
static volatile cyusb_handle * gnDeviceHandle[MAX_DEV_CNT] = {0};

static volatile bool gbDeviceOpened = false;

extern volatile int gbPrintDebugInfo;

typedef struct {
	cyusb_handle * device;
	int endpoint;
} cyusb_endpoint;

static std::vector<cyusb_endpoint*> gnEndpointList;

int GetDeviceCount(int* ven_id, int* dev_id, int numids)
{
	if(gbDeviceOpened)
	{
		printf("Device is opened previously. Return current device count directly.\r\n");
		//gbDeviceOpened = false;
		//cyusb_close();
		return gnDeviceCount;
	}
	//	Get device count. Clear open flags for each device.
	//	Currently do not call cyusb_close() if device is removed.
	gnDeviceCount = 0;
	memset(gnDeviceOpened, 0, MAX_DEV_CNT * sizeof(int));
	memset(gnDeviceHandle, 0, MAX_DEV_CNT * sizeof(cyusb_handle*));
	gnEndpointList.clear();
	if(gbPrintDebugInfo >= LOG_NORMAL)
	{
		char pLog[4096];
		sprintf(pLog, "File USBIO_Linux.cpp Line %5d: Level 1. Try open device(s).\r\n", __LINE__);
		printf(pLog);
		DEV_WriteLogFile(0, 0, (BYTE*)pLog);
	}
	gnDeviceCount = cyusb_open();
	if(gbPrintDebugInfo >= LOG_CRITICAL)
	{
		char pLog[4096];
		if(gnDeviceCount < 0)
			sprintf(pLog, "File USBIO_Linux.cpp Line %5d: Level 2. Device open failed. Return code is %d.\r\n", __LINE__, gnDeviceCount);
		else
			sprintf(pLog, "File USBIO_Linux.cpp Line %5d: Level 2. Total device count: %d\r\n", __LINE__, gnDeviceCount);
		printf(pLog);
		DEV_WriteLogFile(0, 0, (BYTE*)pLog);
	}

	if(gnDeviceCount > 0)
	{
		//	Check each device VID & PID for device detection.
		libusb_device_descriptor desc;
		int nMatchedCount = 0;
		for(int i = 0; i < gnDeviceCount; i++)
		{
			int r = cyusb_get_device_descriptor(cyusb_gethandle(i), &desc);
			if(r == 0)
			{
				for(int j = 0; j < numids; j++)
				{
					if(desc.idVendor == ven_id[j] && desc.idProduct == dev_id[j])
					{
						printf("File USBIO_Linux.cpp Line %5d: Device PID / VID are: %4x / %4x, expected are: %4x / %4x\r\n", __LINE__, desc.idVendor, desc.idProduct, ven_id[j], dev_id[j]);
						gnDeviceIndex[nMatchedCount] = i;
						nMatchedCount++;
						break;
					}
				}
			}
		}

		if(gnDeviceCount > 0 && nMatchedCount == 0)
		{
			gnDeviceCount = 0;
			cyusb_close();
		}
		else
		{
			gnDeviceCount = nMatchedCount;
			gbDeviceOpened = true;
		}
	}
	if(gnDeviceCount == -2)
	{
		char pLog[4096];
		sprintf(pLog, "File USBIO_Linux.cpp Line %5d: LIBUSB CANNOT BE OPENED. CHECK FOR LIBRARY INSTALLATION OR ROOT PRIVILEGE OR OTHER OPENED LIBRARY!\r\n", __LINE__);
		printf(pLog);
		DEV_WriteLogFile(0, 0, (BYTE*)pLog);
		cyusb_close();
	}
	else if(gnDeviceCount <= 0)
	{
		char pLog[4096];
		sprintf(pLog, "File USBIO_Linux.cpp Line %5d: Device not found. Exit.\r\n", __LINE__);
		printf(pLog);
		DEV_WriteLogFile(0, 0, (BYTE*)pLog);
		cyusb_close();
	}
	return gnDeviceCount;
}

void* CreateDeviceHandler(int index)
{
	if(gbDeviceOpened == false)
	{
		GetDeviceCount((int*)g_nCustomFx3VenID, (int*)g_nCustomFx3DevID, g_nCustomFx3DevIDCnt);
		if(gbDeviceOpened == false)
			return NULL;
	}
	printf("File USBIO_Linux.cpp Total device count is %d, requested index is %d.\r\n", gnDeviceCount, index);
	//	Get one device from the device list.
	//	If the device is opened, skip; else return the device.
	if(index >= gnDeviceCount)
	{
		printf("File USBIO_Linux.cpp Requested device exceeds the limitation.\r\n");
		return NULL;
	}
	if(gnDeviceOpened[index])
	{
		printf("File USBIO_Linux.cpp The device is already opened. Return current.\r\n");
		throw -2;
		//return gnDeviceOpened[index];
		//return NULL;
	}
	printf("File USBIO_Linux.cpp Get USB device handle.\r\n");
	cyusb_handle * handle = cyusb_gethandle(gnDeviceIndex[index]);
	//printf("Claim USB interface for device 0x%08x.\r\n", (uint32_t)handle);
	//printf("Libusb status: %d\r\n", libusb_kernel_driver_active(handle, 1));
	int ret = 0;
	//ret = libusb_claim_interface(handle, 0);
	//printf("USB Interface value is %d\r\n", ret);
	//	Try claim handle. If failed, return NULL.
	ret = cyusb_claim_interface(handle, 0);
	printf("File USBIO_Linux.cpp USB Interface value is %d\r\n", ret);
	if(ret != 0)
		handle = NULL;
	else
	{
		gnDeviceOpened[index] = 1;
		gnDeviceHandle[index] = handle;
	}
	return handle;
}

void CloseDeviceHandler(void* devhandle)
{
	//	The handler does not need to be cleared.
}

void CloseDevices()
{
	if(gbDeviceOpened == false)
	{
		return;
	}
	gbDeviceOpened = false;
	//	Close all endpoints.
	int nEndpointSize = gnEndpointList.size();
	for(int i = 0; i < nEndpointSize; i++)
	{
		delete gnEndpointList[i];
	}
	gnEndpointList.clear();
	//	Close all devices. If a device is opened, release the interface.
	for(int i = 0; i < MAX_DEV_CNT; i++)
	{
		if(gnDeviceOpened[i])
			cyusb_release_interface(gnDeviceHandle[i], 0);
	}
	if(gnDeviceCount > 0)
		cyusb_close();
	gnDeviceCount = 0;
	memset(gnDeviceOpened, 0, MAX_DEV_CNT * sizeof(int));
	memset(gnDeviceHandle, 0, MAX_DEV_CNT * sizeof(cyusb_handle*));
}

void* GetEndpoint(void* devhandle, int epnum)
{
	if(gbDeviceOpened == false)
	{
		return NULL;
	}
	cyusb_handle* pdev = (cyusb_handle*)devhandle;
	cyusb_endpoint* endpointset = new cyusb_endpoint();
	endpointset->device = pdev;
	endpointset->endpoint = epnum;
	gnEndpointList.push_back(endpointset);
	return endpointset;
}

bool TransferOutEndpoint(void* epout, BYTE* buffer, int* plen, int timeout)
{
	if(gbDeviceOpened == false)
	{
		*plen = 0;
		return false;
	}
	cyusb_endpoint* endpointset = (cyusb_endpoint*)epout;
	int ret = cyusb_bulk_transfer(endpointset->device, endpointset->endpoint, buffer, *plen, plen, timeout);
	if(ret == 0)
		return true;
	else
		return false;
}

bool TransferInEndpoint(void* epin, BYTE* buffer, int* plen, int timeout)
{
	if(gbDeviceOpened == false)
	{
		*plen = 0;
		return false;
	}
	cyusb_endpoint* endpointset = (cyusb_endpoint*)epin;
	int ret = cyusb_bulk_transfer(endpointset->device, endpointset->endpoint, buffer, *plen, plen, timeout);
	if(ret == 0)
		return true;
	else
		return false;
}

int ResetEndpoint(void* ep)
{
	if(gbDeviceOpened == false)
	{
		return 0;
	}
	cyusb_endpoint* endpointset = (cyusb_endpoint*)ep;
	return cyusb_clear_halt(endpointset->device, endpointset->endpoint);
}

void DownloadFirmware(void* devhandle, char* pfile)
{
	if(gbDeviceOpened == false)
	{
		return;
	}
	cyusb_handle* pdev = (cyusb_handle*)devhandle;
	cyusb_download_fx3(pdev, pfile);
}

void DownloadFirmware(void* devhandle, char* pdata, int datalen)
{
	if(gbDeviceOpened == false)
	{
		return;
	}
	cyusb_handle* pdev = (cyusb_handle*)devhandle;
	FILE* fp = fopen("/tmp/USB3.img", "wb");
	fwrite(pdata, datalen, 1, fp);
	fclose(fp);
	cyusb_download_fx3(pdev, "/tmp/USB3.img");
}

int GetEndpointBurstLength(void* epout)
{
	return 1024;
}