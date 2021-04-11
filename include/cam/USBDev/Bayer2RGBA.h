#ifndef __BAYER_2_RGBA
#define __BAYER_2_RGBA

#include <string.h>

#include "CommonTypes.h"

#ifdef __cplusplus
extern "C"{
#endif

	// Bayer software cropping. Given width, height, dest width, dest height. 
	// The cropped image frame is aligned on even columns and has 4x width and 2x height.
	void RGBASoftCrop(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight, UINT32 cropx, UINT32 cropy);

	void Bayer2RGBA(BYTE* bayersrc, BYTE* RGBAdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty);
	void Bayer2RGB(BYTE* bayersrc, BYTE* RGBAdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty);
	void Bayer2RGB_Split(BYTE* bayersrc, BYTE* RGBdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty);
	void RGB2RGBA(BYTE* rgbsrc, BYTE* RGBAdst, UINT32 width, UINT32 height, BOOL bRBRevert);
	void RGBA2Y(BYTE* rgbaimg, BYTE* rgbadst, UINT32 width, UINT32 height);
	void RGBA2YEnergy(BYTE* rgbaimg, BYTE* rgbadst, UINT32 width, UINT32 height);
	void RGBA2Y_RAW(BYTE* rgbaimg, BYTE* rawdst, UINT32 width, UINT32 height);

	void RGBA2Bayer(BYTE* rgbasrc, BYTE* bayerdst, UINT32 width, UINT32 height);

	void RGBAScale(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight);
	void RGBAZoom(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight);

	UINT32 SaveBMP(BYTE* rgbasrc, BYTE* dstfile, UINT32 width, UINT32 height, UINT32 iscolor);

	//	Save Wide Bayer BMP. Used for WDR only. 
	//	The WIDTH parameter here should be the original width value. Program will divide by 2 internally.
	UINT32 SaveWBayerBMP(BYTE* bayersrc, BYTE* dstfile, UINT32 width, UINT32 height, UINT32 dbits);
	void SaveRawBayer(BYTE* bayersrc, char* dstfile, UINT32 width, UINT32 height);
	void SaveRawBayerASCII(BYTE* bayersrc, char* dstfile, UINT32 width, UINT32 height);
	void SaveRGBBin(BYTE* rgbsrc, char* dstfile, UINT32 width, UINT32 height);

	void Bayer2RGB_Crop(BYTE* bayersrc, UINT32 orgwidth, UINT32 orgheight, BYTE* RGBdst, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty, UINT32 shiftx, UINT32 shifty);
	void Bayer2RGBA_Crop(BYTE* bayersrc, UINT32 orgwidth, UINT32 orgheight, BYTE* RGBdst, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty, UINT32 shiftx, UINT32 shifty);
	void Gray2RGBA_Crop(BYTE* graysrc, UINT32 orgwidth, UINT32 orgheight, BYTE* RGBAdst, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty, UINT32 shiftx, UINT32 shifty);

	void Bayer2RGB_OpenCV(BYTE* bayersrc, BYTE* RGBdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty);
	void Bayer2RGBA_OpenCV(BYTE* bayersrc, BYTE* RGBdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty);

	void AutoWBCalibrate(BYTE* rgbasrc, UINT32 width, UINT32 height, double* rgbscale);
	void PerformAutoWB(BYTE* rgbasrc, UINT32 width, UINT32 height);

	void DoFlip(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 xflip, UINT32 yflip);
	UINT32 CalculateMeanEnergy(BYTE* rgbasrc, UINT32 xstart, UINT32 ystart, UINT32 width, UINT32 height);

	void BayerPixelNormalizeTemplateGen(BYTE* bayersrc, USHORT* templ, UINT32 width, UINT32 height);
	void BayerPixelNormalize(BYTE* bayersrc, USHORT* templ, UINT32 width, UINT32 height);

	void ConvertSaveRawFile(BYTE* srcfile, BYTE* dstfile, UINT32 width, UINT32 height);
	UINT32 LoadBMP(BYTE* srcfile, BYTE* rgbadst, UINT32* pwidth, UINT32* pheight);
	UINT32 LoadVerifyBMP(BYTE* srcfile, BYTE* rgbadst, UINT32 width, UINT32 height);
	void AddVisualBars(BYTE* rgbafrm, UINT32 width, UINT32 height);

	void Byte2RGBA(BYTE* bytesrc, BYTE* rgbadst, UINT32 width, UINT32 height, BOOL bRBRevert);

	//	TOF decoder. The source data is overwritten.
	void DecodeTOF(BYTE* bytesrc, UINT32* width, UINT32* height, UINT32 offset, UINT32 usetemporalfilter, UINT32 temporalthreshold, UINT32 usespatialfilter, UINT32 spatialthreshold);
	void DecodeOPT9221(BYTE* bytesrc, UINT32* width, UINT32* height);

	//	TOF filter. Extract valid frames from frame list.
	//	Filter TOF data. The returned is NULL if no valid data selected. pvalidsf points to valid sub frame count. Return 0 if failed. bytedst is required to be created outside.
	UINT32 FilterTOFData(BYTE* bytesrc, BYTE* phasedst, UINT32 width, UINT32 height, UINT32 filterxorg, UINT32 filteryorg, UINT32 filterwidth, UINT32 filterheight, UINT32 threshillum, UINT32 threshphase, UINT32 threshpixel, UINT32 tofoffset, UINT32 tofmodfreq, UINT32 threshspatial, BYTE** bytedst, UINT32* pvalidsf);	

	//	ROI for byte frame. If the given ROI is out of boundary, the output is then blacked.
	//	The bytedst can be the same array with bytesrc.
	void ROI8(BYTE *bytesrc, BYTE *bytedst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty);

	//	Pixel Mapping. The palette is 256 bytes.
	//	The bytedst can be the same array with bytesrc.
	void Map8(BYTE *bytesrc, BYTE *bytedst, UINT32 width, UINT32 height, BYTE *palette);

	//	Median Filter.
	//	Operates on grayscale images only (pixel occupies 1 pixel).
	void MedianFilter8(BYTE *bytesrc, BYTE *bytedst, UINT32 width, UINT32 height);


#ifdef __cplusplus
}
#endif

#endif