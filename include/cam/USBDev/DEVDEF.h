#ifndef __DEV_DEFINITION
#define __DEV_DEFINITION

#include "CommonTypes.h"

//	All supported device lists.
const int g_nCustomFx3VenID[] = {0x04B4};
const int g_nCustomFx3DevID[] = {0x0082};
const int g_nCustomFx3DevIDCnt = 1;

//	Device specific parameters
#define	MAX_DEV_CNT		16		//	Number of max supported devices
#define	MAX_STREAM_CNT	2		//	Number of max supported streams in each device
#define	MAX_VIRTCH_CNT	6		//	Number of max supported streams in each device
#define	SZSTREAM_BUFFER	524288	//	512 KB

#define	STREAM_FAST_SYNC	0x19F427D0

// End Point Definition
#define	END_POINT_CMD_OUT		0x06
#define	END_POINT_RESP_IN		0x88
#define	END_POINT_STREAM_IN	0x82


//////////////////////////////////////////////////////////////////////////////////
//	Command type header
//////////////////////////////////////////////////////////////////////////////////

//	Note each packet contains 4 bytes type header.

//	Verify USB. 
#define	CMD_TYPE_VERIFY	(0x60)	//	USB Command Group

#define	CMD_TYPE_IIC	(0x65)	//	IIC Command Group
//	All IIC commands must follow the rules:
//	[0~3] CMD_TYPE_IIC [4] CMD_IIC_XX [8] IIC_INDEX [9] IIC_SLVADDR [A] IIC_RALEN [B] IIC_DALEN_L [C] IIC_DALEN_H [D] IIC_RADDR [D+M] IIC_DATA (if any)
//	Up to 256 bytes may be read / write at a single time.
#  define	CMD_IIC_READ	(0x01)
#  define	CMD_IIC_WRITE	(0x02)

#define	CMD_TYPE_USB	(0x66)
//	Read DNA
#  define	CMD_USB_READDNA	(0x01)

#define	CMD_TYPE_SENSOR	(0x67)
//	The trigger uses GPIO_GPR_0 for read / write.
//	The command sequence should be 0x5A, Cmd, Byte 3, Byte 2, Byte 1, Byte 0, CRC. The CRC is invert of sum of cmd and data.
//	[0~3] CMD_TYPE_SENSOR [4] CMD_TRIGGER_XX [8] CMD [9] BYTE3 [10] BYTE2 [11] BYTE1 [12] BYTE0 [13] CRC
#  define	CMD_TRIGGER_WRITE	(0x02)
//	[10:9] EXT TRIGGER MODE [8] TRIGGER MODE [1] LED POLARY [0] LED MODE
#  define	CMD_TRIGGER_READ	(0x03)

#define	CMD_TYPE_SPI	(0x69)
//	All SPI commands must follow the fules:
//	[0~3] CMD_TYPE_SPI [4] CMD_SPI_XX [8] SPI_INDEX [9] ADDRLEN [10] DALEN_L [11] DALEN_H [12] ADDR [12+M] DATA
#  define	CMD_SPI_READ	(0x01)
#  define	CMD_SPI_WRITE	(0x02)
#  define	CMD_SPI_WAITIDLE	(0x11)



#define DEFAULT_MAX_FRAME_WIDTH	2048
#define DEFAULT_MAX_FRAME_HEIGHT	1536
extern int DEV_MAX_FRAME_WIDTH;
extern int DEV_MAX_FRAME_HEIGHT;

#endif