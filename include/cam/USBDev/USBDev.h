#ifndef __SIRDEVICE
#define __SIRDEVICE

#include "CommonTypes.h"

#ifdef __cplusplus
extern "C"{
#endif


	enum DEV_PARAM
	{
		SET_LOG_LEVEL = 1,	//	Set Log Level. 

		//	LED_OUT control
		SET_LED_MODE,		//	LED Mode. 0: Follow LED_OUT  1: LED_OUT Trigger Mode
		SET_LED_POLARY,		//	LED Polary. 0: High Valid  1: Low Valid
		SET_LED_ON_TIME,		//	LED Output On Time (in clock cycles)

		//	Trigger control
		SET_TRIGGER_MODE,		//	Trigger Mode. 0: Disable Trigger  1: Internal Trigger  2: Manual Trigger
		SET_TRIGGER_PIN_MODE,	//	Trigger Pin Mode. 0: Rising Edge  1: Falling Edge  2: High Level  3: Low Level
		SET_TRIGGER_ONCE,		//	Trigger Once. 1 to trigger once.
		SET_TRIGGER_PERIOD,	//	Internal Trigger Period (in clock cycles)

		//	Register R / W
		//	Read data is in byte sequence, while write now supports up to 4 byte data which is in little endian format, e.g. 0x0102 should be written just as 0x00000102.
		SET_SENSOR_REG,		//	Write Sensor Register. [0] Bus  [1] Slave Addr  [2] Reg Addr  [3] Reg Data  [4] RA Len  [5] DA Len
		GET_SENSOR_REG,		//	Read Sensor Register. Write: [0] Bus  [1] Slave Addr  [2] Reg Addr  [3] Dummy  [4] RA Len  [5] DA Len  Read: [0] to [N] Read Data

		SET_034_ANALOG_GAIN,	//	Set 034 Analog Gain. [0] AGC Enable  [1] Gain  [2] Max Gain for AGC
		SET_034_EXPOSURE,		//	Set 034 Exposure. [0] AEC Enable  [1] HDR Enable  [2] Auto Exposure Knee Adjust Enable  [3] Coarse Int  [4] Knee 1  [5] Knee 2  [6] Min Int for AEC  [7] Max Int for AEC
		SET_034_DESIREDBIN,	//	Set 034 Desired Bin. [0] Bin

		GET_034_AECAGCSTATUS,	//	Get 034 AEC / AGC Status. Bit 0 AEC  Bit 1 AGC
		GET_034_AECMINMAXINT,	//	Get 034 Min / Max Integration. [0] Min  [1] Max
		GET_034_TOTALINTEG,	//	Get 034 Total Integration. [0] Integration
		GET_034_KNEEPOINTS,	//	Get 034 Knee Points (Exposure) (HDR only) [0] Knee 1  [1] Knee 2
		GET_034_AGC_MAXGAIN,	//	Get 034 AGC Max Gain. [0] Gain
		GET_034_ANALOGGAIN,	//	Get 034 Analog Gain. [0] Gain
		GET_034_DESIREDBIN,	//	Get 034 Desired Bin. [0] Bin
		GET_034_HDRSTATE,		//	Get 034 HDR State. 0: Disabled  1: Enabled
		GET_034_KNEEAUTOADJ,	//	Get 034 Exposure Knee Auto Adjust 0: Disable  1: Enable

		SET_034_RESET,		//	Reset 034. 1: reset  0: do nothing
		SET_SENSOR_RESET,		//	Hard reset sensor. 1: reset  0: do nothing

		SET_034_BINMODE,		//	Set 034 bin mode. 0: normal  1: 2x2  2: 4x4  Software based horizontal binning

		SET_034_RESETPIN,		//	Reset 034 (full). 1: reset  0: do nothing. Default to reset mode.
		SET_034_INITIALIZE,	//	Initialize 034. Must be called after Reset clear.

		SET_PWM_MODE,		//	Set PWM mode. [0] Index (0, 1)  [1] Period (T - 1)  [2] Duty (D)  [3] Enable
	};

	enum DEV_STATUS
	{
		DEV_SUCCESS = 0,
		DEV_FAIL,
		DEV_DEVICENOTFOUND,
		DEV_DEVICENOTOPEN,
		DEV_OTHERFAILURE,
		DEV_UNSUPPORTED,
		DEV_ALREADYOPENED,
		DEV_ALREADYCLOSED,
		DEV_NOTAVAILABLE,		//	Command not supported by the hardware
		DEV_FWOUTDATED,		//	Firmware needs to be updated.
		DEV_BUSY,			//	The device is busy now
		DEV_NOACK,			//	No ACK received
		DEV_SENSORNOTMATCH,	//	Sensor type not match
		DEV_UNAUTHORIZED,		//	Device is not authorized
		DEV_INVALIDPARAM,		//	Input parameter of function call is invalid
		DEV_HARDWAREERROR,	//	Hardware error detected. Bit test only. Should contact manufacturer.
		DEV_INITFAILED,		//	Initialize failed
		DEV_FWFILENOTFOUND,	//	Firmware file not found
		DEV_DISCONNECTED,		//	Device is disconnected. Currently used in device monitor thread.
		DEV_NOTRESPONDING,	//	Device is not responding. Currently used in device monitor thread.
		DEV_FIRMWAREERROR,	//	Firmware error detected. Should contact manufacturer.
		DEV_USBFWNOTREADY,	//	USB firmware not ready. Check for USB firmware availability.
		DEV_OUTOFINDEX,		//	Input array index exceeds the limit.
		DEV_BUFOVERFLOW,		//	Input buffer overflow.
		DEV_BUFINVALID,		//	Invalid frame buffer.
	};

	enum DEV_SENSORMODE
	{
		MODE_NORMAL = 0,
		MODE_2X2BIN,
		MODE_4X4SKIP,
		MODE_VGA,			
	};

	typedef struct {
		int year;
		int month;
		int day;
		int hour;
		int minute;
		int second;
		int millisecond;
	} USBDev_Time;

	USBDev_Time DEV_GetCurrentTime();

#pragma pack(push)
#pragma pack(1)

	typedef struct 
	{
		/* Frame Buffer. 
		 * Contains the returned frame buffer pointer. 
		 * Do not delete it.
		 */
		BYTE* pFrameBuffer;
		/* Frame Width & Height Information.
		 */
		UINT32 nFrameWidth;
		UINT32 nFrameHeight;
		/* Frame Timestamp.
		 * The frame acquisition time (in millisecond).
		 * Can be used to track time of the frame taken, or to obtain iris images by time.
		 */
		UINT32 nTimeStamp;
		/* Raw Frame Rate Acquisition
		 * nFrameCycle is the number of SOC clock cycles between 2 frames.
		 * nCycleResolution is the number of SOC clock cycles in a millisecond.
		 * The frame time can be calculated as (nFrameCycle / nCycleResolution) ms.
		 * The final frame rate is then 1000 / (nFrameCycle / nCycleResolution).
		 */
		UINT32 nFrameCycle;
		UINT32 nCycleResolution;
		/* Frame Valid
		 * If the frame is invalid (e.g. buffer overflow, device failed, etc), bFrameValid is 0.
		 */
		UINT32 bFrameValid;
	} DEV_FRAME_INFO;

#pragma pack(pop)

	//	The log level is splitted to critical, required, normal, unimportant, call, and all.
	//	The log categorization is required.
	enum LOG_LEVEL
	{
		LOG_CRITICAL = 0,		//	Critical information. Including error returning codes & operation error codes.
		LOG_REQUIRED,		//	Required information. Including device initialization details, etc.
		LOG_NORMAL,			//	Normal information. Including necessary information about the devices.
		LOG_UNIMPORTANT,		//	Unimportant information. Including some information in the startup, etc.
		LOG_CALL,			//	Call information. Log every single calls to each device separately. Will slow down system performance.
		LOG_ALL = 99,		//	All information. Log everything including frame information.
	};

	/* Method get device count.
	*	Return supported device count.
	*/
	int DEV_QueryDevice();

	/* Method open devices.
	*	Open all devices.
	* Return: DEV_SUCCESS if OK, DEV_FAIL if either one failed.
	*/
	DEV_STATUS DEV_OpenDevice();

	/* Method get device masked list.
	*	Get opened device masked list. Each bit indicates one opened device.
	* Return: Masked List.
	*/
	UINT32 DEV_GetDeviceList();

	/* Method close devices.
	*	Close all devices.
	* Return: DEV_SUCCESS if OK, DEV_FAIL if either one failed.
	*/
	DEV_STATUS DEV_CloseDevice();

	/* Method set device parameter.
	*	Set masked devices parameter.
	* Input:
	*	uDeviceMask: Mask for devices. b2..b0: Device 3 .. Device 1. 1: enable, 0: disable.
	*	uParamType: Parameter type
	*	uParamValue: Parameter value. For 16 bit data transfer, only lower 16 bits are used.
	*			 If more than one DWORD is used, the parameter may represent an array.
	* Return: DEV_SUCCESS if OK, DEV_FAIL if either one failed, DEV_UNSUPPORTED if not supported (e.g. give GET_XXX method)
	* Note: SET_CMD_COMMIT command should be called once after all parameters are properly set.
	*/
	DEV_STATUS DEV_SetDeviceParameter(UINT32 uDeviceMask, UINT32 nParamType, UINT32* uParamValue);

	/* Method get device parameter.
	*	get masked devices parameter.
	* Input:
	*	uDeviceMask: Mask for devices. b2..b0: Device 3 .. Device 1. 1: enable, 0: disable.
	*	uParamType: Parameter type
	*	uParamValue: Pointer to parameter value. For 16 bit data transfer, only lower 16 bits are used.
	*			 If more than one DWORD is used, the parameter may represent an array.
	* Return: DEV_SUCCESS if OK, DEV_FAIL if either one failed.
	* Note: GET_CMD_COMMIT command should be called once after all parameters are properly set.
	*/
	DEV_STATUS DEV_GetDeviceParameter(UINT32 uDeviceMask, UINT32 uParamType, UINT32* uParamValue);

	/* Method get stream.
	*	Universal Get Frame.
	* Input:
	*	uDeviceIndex: Device index (start from 0)
	*	uStreamIndex: Stream index (start from 0)
	*	uVirtualChIndex: Virtual channel index (start from 0)
	*	uStreamType: Pointer to stream type. 0: 8-bit  1: 16-bit  
	*	pFrameInfo: Pointer to a DEV_FRAME_INFO struct
	*/
	DEV_STATUS DEV_GetFrame(UINT32 uDeviceIndex, UINT32 uStreamIndex, UINT32 uVirtualChIndex, DEV_FRAME_INFO* pFrameInfo);

	/* Method get last error information.
	*	Get last error information.
	* Return: char array containing the exact error information.
	*/
	char* DEV_GetLastError();

	/* Method to log someting
	* Input:
	*	logtype: 0 for global log, and 1 for per device log.
	*	devindex: device index
	*	logstr: log string
	* Return: 0 if succeeded, other if failed.
	*/
	int DEV_WriteLogFile(UINT32 logtype, UINT32 devindex, BYTE* logstr);

#ifdef __cplusplus
}
#endif

#endif