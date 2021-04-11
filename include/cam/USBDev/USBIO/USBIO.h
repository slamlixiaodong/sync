#ifndef __USBIO
#define __USBIO

#include "../CommonTypes.h"

extern BYTE g_bUSB3[131072];

/* Get Device Count.
* Get device count for given vendor id and device id.
* Should always be called previously before CreateDeviceHandler.
*/
int GetDeviceCount(int* ven_id, int* dev_id, int numids);

/* Create Device Handler.
* Create one device handler from sub.
* Users should not replace the handler with some other things.
* Return NULL if exceeds the index or device failed to open.
* GetDeviceCount will be called internally if the global device count is 0.
*/
void* CreateDeviceHandler(int index);

/* Close Device & Drop Handler.
* Releases all resources used by this handler.
*/
void CloseDeviceHandler(void* devhandle);

/* Close Devices.
* Function valid in linux only.
*/
void CloseDevices();

/* Get EndPoint Handler.
* get one endpoint handler from sub.
* Users should not replace the handler with some other things.
* Return NULL if the specific endpoint does not exist.
*/
void* GetEndpoint(void* devhandle, int epnum);

/* Transfer OUT endpoint.
* Transfer *plen bytes and return status of this operation.
* plen contains the effective written length.
*/
bool TransferOutEndpoint(void* epout, BYTE* buffer, int* plen, int timeout);

/* Transfer IN endpoint.
* Transfer *plen bytes and return status of this operation.
* plen contains the effective written length.
*/
bool TransferInEndpoint(void* epin, BYTE* buffer, int* plen, int timeout);

/* Reset EndPoint
*/
int ResetEndpoint(void* ep);

/* Download Firmware
*/
void DownloadFirmware(void* devhandle, char* pfile);

/* Download Firmware by array
*/
void DownloadFirmware(void* devhandle, char* pdata, int datalen);

/* Get Endpoint Burst Length
 * Always use EPOUT CMD for test.
*/
int GetEndpointBurstLength(void* epout);

/* Check Device Online State
 * Return true if online, false if offline.
*/
bool CheckDeviceOnline(void* devhandle);

#endif