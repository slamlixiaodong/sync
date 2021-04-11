#include "CommonTypes.h"

#include "Bayer2RGBA.h"
#include <stdio.h>
#include "DEVDEF.h"

#define __FRM_MAX_WIDTH		MAX_FRAME_WIDTH
#define __FRM_MAX_HEIGHT	MAX_FRAME_HEIGHT

// Get Line Data. Input of lineno from -1 to height, while the actual returned line is 1, 0, ..., h - 1, h - 2.
__inline BYTE* GetLine(BYTE* pbuffer, int lineno, int width, int height, int pixskip)
{
	if(lineno == -1)
		return pbuffer + width * pixskip;
	else if(lineno == height)
		return pbuffer + (lineno - 2) * width * pixskip;
	else if(lineno >= 0 && lineno < height)
		return pbuffer + lineno * width * pixskip;
	else
		return NULL;
}

// (cropx, cropy) is the start point. The output is dstwidth and dstheight.
void RGBASoftCrop(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight, UINT32 cropx, UINT32 cropy)
{
	for(int i = cropy; i < cropy + dstheight; i++)
	{
		BYTE* srcrow = GetLine(rgbasrc, i, width, height, true);
		BYTE* dstrow = GetLine(rgbadst, i - cropy, dstwidth, dstheight, true);
		memcpy(dstrow, srcrow + 4 * cropx, 4 * dstwidth);
	}
}

void Complement2FullScale(BYTE* bayersrc, UINT32 width, UINT32 height)
{
	for(int i = 0; i < height; i++)
	{
		BYTE* pBayerLine = GetLine(bayersrc, i, width, height, 1);
		for(int j = 0; j < width; j++)
		{
			int val = ((pBayerLine[j] >= 96) ? pBayerLine[j] - 96 : 0) * 4;
			if(val < 0)
				val = 0;
			if(val > 255)
				val = 255;
			pBayerLine[j] = val;
		}
	}
}

void RGBA2Y_RAW(BYTE* rgbaimg, BYTE* rawdst, UINT32 width, UINT32 height)
{
#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0
#define A_OFFSET 3
	for(int y = 0; y < height; y++)
	{
		BYTE * pLine = GetLine(rgbaimg, y, width, height, 4);
		BYTE * pDst = GetLine(rawdst, y, width, height, 1);
		for(int x = 0; x < width; x++)
		{
			double yd = 0.257 * pLine[x * 4 + R_OFFSET] + 0.564 * pLine[x * 4 + G_OFFSET] + 0.098 * pLine[x * 4 + B_OFFSET] + 16;
			pDst[x] = yd;
		}
	}
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
}

void RGBA2Bayer(BYTE* rgbasrc, BYTE* bayerdst, UINT32 width, UINT32 height)
{
	//	Generate standard RGGB pattern.
#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0
#define A_OFFSET 3
	for(int i = 0; i < height; i++)
	{
		BYTE* prgbarow = GetLine(rgbasrc, i, width, height, 4);
		BYTE* pbayerrow = GetLine(bayerdst, i, width, height, 1);
		//	Row 0: R G, Row 1: G B
		int j0offset = ((i % 2 == 0) ? G_OFFSET : B_OFFSET);
		int j1offset = ((i % 2 == 0) ? R_OFFSET : G_OFFSET);
		for(int j = 0; j < width - 1; j += 2)
		{
			pbayerrow[j + 0] = prgbarow[j * 4 + j0offset];
			pbayerrow[j + 1] = prgbarow[j * 4 + j1offset];
		}
	}
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
}

static void Bayer2RGB(BYTE* bayersrc, BYTE* RGBAdst, UINT32 width, UINT32 height, int nSrcPixSkip, int nDstPixSkip, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty, UINT32 shiftx, UINT32 shifty, UINT32 bSplitRGB = 0, UINT32 Gray = 0)
{
	//Complement2FullScale(bayersrc, width, height);
	// If width or height is not even, do nothing.
	if(dstheight > height)
		dstheight = height;
	if(dstwidth > width)
		dstwidth = width;
	if(height - dstheight < starty)
		starty = height - dstheight;
	if(width - dstwidth < startx)
		startx = width - dstwidth;
	if(bSplitRGB)
		nDstPixSkip = 1;
	bool bDstRGBA = (bSplitRGB == 0) && (nDstPixSkip == 4);
	//printf("Interpolation Begin.\r\n");
#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0
#define A_OFFSET 3
	// The buffer holds up to 3 lines
	for(int i = starty; i < starty + dstheight; i++)
	{
		// Fetch rgba line
		BYTE *pRLine = NULL, *pGLine = NULL, *pBLine = NULL, *pALine = NULL;
		if(bSplitRGB)
		{
			pRLine = GetLine(RGBAdst, i - starty, dstwidth, dstheight * 3, nDstPixSkip);
			pGLine = GetLine(RGBAdst, i - starty + dstheight, dstwidth, dstheight * 3, nDstPixSkip);
			pBLine = GetLine(RGBAdst, i - starty + dstheight * 2, dstwidth, dstheight * 3, nDstPixSkip);
		}
		else
		{
			pRLine = GetLine(RGBAdst, i - starty, dstwidth, dstheight, nDstPixSkip);
			pGLine = GetLine(RGBAdst, i - starty, dstwidth, dstheight, nDstPixSkip);
			pBLine = GetLine(RGBAdst, i - starty, dstwidth, dstheight, nDstPixSkip);
			pALine = GetLine(RGBAdst, i - starty, dstwidth, dstheight, nDstPixSkip);
		}
		// Fetch 3 bayer lines
		BYTE* pBayer[3] = {NULL, NULL, NULL};
		for(int j = 0; j < 3; j++)
			pBayer[j] = GetLine(bayersrc, i - 1 + j, width, height, nSrcPixSkip);
		// Do calculation based on odd or even of i.
		if(((i + shifty) & 1) == 0)
		{
			int j = startx;
			if(j == 0)
			{
				if((shiftx & 1) == 0)
				{
					// Calculate column 0
					pGLine[G_OFFSET] = pBayer[1][0 * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : pBayer[1][1 * nSrcPixSkip];
					pBLine[B_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : (pBayer[0][0 * nSrcPixSkip] + pBayer[2][0 * nSrcPixSkip]) >> 1;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				else
				{
					pRLine[R_OFFSET] = pBayer[1][0 * nSrcPixSkip];
					pGLine[G_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : (pBayer[1][1 * nSrcPixSkip] + pBayer[1][1 * nSrcPixSkip] + pBayer[0][0 * nSrcPixSkip] + pBayer[2][0 * nSrcPixSkip]) >> 2;
					pBLine[B_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : (pBayer[0][1 * nSrcPixSkip] + pBayer[2][1 * nSrcPixSkip]) >> 1;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				pRLine += nDstPixSkip;
				pGLine += nDstPixSkip;
				pBLine += nDstPixSkip;
				j++;
			}
			int interpox = startx + dstwidth;
			if(interpox >= width - 1)
				interpox = width - 1;
			for(; j < interpox; j++)
			{
				if(((j + shiftx) & 1) == 0)
				{
					pGLine[G_OFFSET] = pBayer[1][j * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[1][(j + 1) * nSrcPixSkip]) >> 1;
					pBLine[B_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 1;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				else
				{
					pRLine[R_OFFSET] = pBayer[1][j * nSrcPixSkip];
					pGLine[G_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[1][(j + 1) * nSrcPixSkip] + pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 2;
					pBLine[B_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][(j - 1) * nSrcPixSkip] + pBayer[0][(j + 1) * nSrcPixSkip] + pBayer[2][(j - 1) * nSrcPixSkip] + pBayer[2][(j + 1) * nSrcPixSkip]) >> 2;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				pRLine += nDstPixSkip;
				pGLine += nDstPixSkip;
				pBLine += nDstPixSkip;
			}
			if(j == width - 1)
			{
				if((shiftx & 1) == 0)
				{
					// Calculate column N - 1
					pRLine[R_OFFSET] = pBayer[1][(j) * nSrcPixSkip];
					pGLine[G_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 2;
					pBLine[B_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][(j - 1) * nSrcPixSkip] + pBayer[2][(j - 1) * nSrcPixSkip]) >> 1;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				else
				{
					pGLine[G_OFFSET] = pBayer[1][j * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[1][(j - 1) * nSrcPixSkip]) >> 1;
					pBLine[B_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 1;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
			}
		}
		else
		{
			// Odd columns
			int j = startx;
			if(j == 0)
			{
				if((shiftx & 1) == 0)
				{
					// Calculate column 0
					pBLine[B_OFFSET] = pBayer[1][0 * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : (pBayer[0][1 * nSrcPixSkip] + pBayer[2][1 * nSrcPixSkip]) >> 1;
					pGLine[G_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : ((pBayer[1][1 * nSrcPixSkip] << 1) + pBayer[0][0 * nSrcPixSkip] + pBayer[2][0 * nSrcPixSkip]) >> 2;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				else
				{
					pGLine[G_OFFSET] = pBayer[1][0 * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : (pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 1;
					pBLine[B_OFFSET] = Gray ? pBayer[1][0 * nSrcPixSkip] : (pBayer[1][1] + pBayer[1][1]) >> 1;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				pRLine += nDstPixSkip;
				pGLine += nDstPixSkip;
				pBLine += nDstPixSkip;
				j++;
			}
			int interpox = startx + dstwidth;
			if(interpox >= width - 1)
				interpox = width - 1;
			for(; j < interpox; j++)
			{
				if(((j + shiftx) & 1) == 0)
				{
					pBLine[B_OFFSET] = pBayer[1][j * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][(j - 1) * nSrcPixSkip] + pBayer[0][(j + 1) * nSrcPixSkip] + pBayer[2][(j - 1) * nSrcPixSkip] + pBayer[2][(j + 1) * nSrcPixSkip]) >> 2;
					pGLine[G_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[1][(j + 1) * nSrcPixSkip] + pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 2;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				else
				{
					pGLine[G_OFFSET] = pBayer[1][j * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 1;
					pBLine[B_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[1][j - 1] + pBayer[1][j + 1]) >> 1;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				pRLine += nDstPixSkip;
				pGLine += nDstPixSkip;
				pBLine += nDstPixSkip;
			}
			if(j == width - 1)
			{
				if((shiftx & 1) == 0)
				{
					// Calculate column N - 1
					pGLine[G_OFFSET] = pBayer[1][(width - 1) * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][(width - 1) * nSrcPixSkip] + pBayer[2][(width - 1) * nSrcPixSkip]) >> 1;
					pBLine[B_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : pBayer[1][(width - 2) * nSrcPixSkip];
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
				else
				{
					pBLine[B_OFFSET] = pBayer[1][j * nSrcPixSkip];
					pRLine[R_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[0][(j - 1) * nSrcPixSkip] + pBayer[0][(j - 1) * nSrcPixSkip] + pBayer[2][(j - 1) * nSrcPixSkip] + pBayer[2][(j - 1) * nSrcPixSkip]) >> 2;
					pGLine[G_OFFSET] = Gray ? pBayer[1][j * nSrcPixSkip] : (pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[1][(j - 1) * nSrcPixSkip] + pBayer[0][j * nSrcPixSkip] + pBayer[2][j * nSrcPixSkip]) >> 2;
					if(bDstRGBA) pALine[A_OFFSET] = 0;
				}
			}
		}
	}
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
	//printf("Interpolation End.\r\n");
}


void DoFlip(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 xflip, UINT32 yflip)
{
	//	Select a line and select destination line.
	UINT32* pLineBuf = new UINT32[width];
	for(int i = 0; i < height; i++)
	{
		UINT32* pLine = (UINT32*)GetLine(rgbasrc, i, width, height, 4);
		UINT32* pDstLine = NULL;
		if(yflip)
			pDstLine = (UINT32*)GetLine(rgbadst, (height - 1 - i), width, height, 4);
		else
			pDstLine = (UINT32*)GetLine(rgbadst, i, width, height, 4);
		//	Do flip
		for(int j = 0; j < width; j++)
		{
			if(xflip)
				pLineBuf[width - 1 - j] = pLine[j];
			else
				pLineBuf[j] = pLine[j];
		}
		for(int j = 0; j < width; j++)
		{
			pDstLine[j] = pLineBuf[j];
		}
	}
	delete []pLineBuf;
}

UINT32 CalculateMeanEnergy(BYTE* rgbasrc, UINT32 xstart, UINT32 ystart, UINT32 width, UINT32 height)
{
#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0
#define A_OFFSET 3
	double ysum = 0;
	for(int y = ystart; y < ystart + height; y++)
	{
		BYTE * pLine = GetLine(rgbasrc, y, width, height, 4);
		for(int x = xstart; x < xstart + width; x++)
		{
			ysum +=  0.257 * pLine[x * 4 + R_OFFSET] + 0.564 * pLine[x * 4 + G_OFFSET] + 0.098 * pLine[x * 4 + B_OFFSET] + 16;
		}
	}
	ysum /= ((width - 1) * height);
	return ysum;
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
}

void RGBA2YEnergy(BYTE* rgbaimg, BYTE* rgbadst, UINT32 width, UINT32 height)
{
#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0
#define A_OFFSET 3
	for(int y = 0; y < height; y++)
	{
		BYTE * pLine = GetLine(rgbaimg, y, width, height, 4);
		BYTE * pDst = GetLine(rgbadst, y, width, height, 4);
		for(int x = 0; x < width; x++)
		{
			double yd = 0.257 * pLine[x * 4 + R_OFFSET] + 0.564 * pLine[x * 4 + G_OFFSET] + 0.098 * pLine[x * 4 + B_OFFSET] + 16;
			double yr = 255.0 - yd * 2;
			double yg = yd * 2;
			if(yg > 255)
				yg = 511 - yg;
			double yb = -255.0 + yd * 2;
			if(yr < 0)
				pDst[x * 4 + R_OFFSET] = 0;
			else
				pDst[x * 4 + R_OFFSET] = yr;
			if(yg > 255)
				pDst[x * 4 + G_OFFSET] = 255;
			else
				pDst[x * 4 + G_OFFSET] = yg;
			if(yb < 0)
				pDst[x * 4 + B_OFFSET] = 0;
			else
				pDst[x * 4 + B_OFFSET] = yb;
		}
	}
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
}

void RGBA2Y(BYTE* rgbaimg, BYTE* rgbadst, UINT32 width, UINT32 height)
{
#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0
#define A_OFFSET 3
	for(int y = 0; y < height; y++)
	{
		BYTE * pLine = GetLine(rgbaimg, y, width, height, 4);
		BYTE * pDst = GetLine(rgbadst, y, width, height, 4);
		for(int x = 0; x < width; x++)
		{
			double yd = 0.257 * pLine[x * 4 + R_OFFSET] + 0.564 * pLine[x * 4 + G_OFFSET] + 0.098 * pLine[x * 4 + B_OFFSET] + 16;
			pDst[x * 4 + R_OFFSET] = yd;
			pDst[x * 4 + G_OFFSET] = yd;
			pDst[x * 4 + B_OFFSET] = yd;
		}
	}
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
}

void RGB2RGBA(BYTE* rgbsrc, BYTE* RGBAdst, UINT32 width, UINT32 height, BOOL bRBRevert)
{
#define R_OFFSET 0
#define G_OFFSET 1
#define B_OFFSET 2
#define A_OFFSET 3
	for(int i = 0; i < height; i++)
	{
		BYTE* pRGBLine = GetLine(rgbsrc, i, width, height, 3);
		BYTE* pRGBALine = GetLine(RGBAdst, i, width, height, 4);
		for(int j = 0; j < width; j++)
		{
			pRGBALine[R_OFFSET + j * 4] = pRGBLine[(bRBRevert ? B_OFFSET : R_OFFSET) + j * 3];
			pRGBALine[G_OFFSET + j * 4] = pRGBLine[G_OFFSET + j * 3];
			pRGBALine[B_OFFSET + j * 4] = pRGBLine[(bRBRevert ? R_OFFSET : B_OFFSET) + j * 3];
			pRGBALine[A_OFFSET + j * 4] = 0xFF;
		}
	}
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
}

void Byte2RGBA(BYTE* bytesrc, BYTE* rgbadst, UINT32 width, UINT32 height, BOOL bRBRevert)
{
#define R_OFFSET 0
#define G_OFFSET 1
#define B_OFFSET 2
#define A_OFFSET 3
	for(int i = 0; i < height; i++)
	{
		BYTE* pRGBLine = GetLine(bytesrc, i, width, height, 1);
		BYTE* pRGBALine = GetLine(rgbadst, i, width, height, 4);
		for(int j = 0; j < width; j++)
		{
			pRGBALine[R_OFFSET + j * 4] = pRGBLine[j];
			pRGBALine[G_OFFSET + j * 4] = pRGBLine[j];
			pRGBALine[B_OFFSET + j * 4] = pRGBLine[j];
			pRGBALine[A_OFFSET + j * 4] = 0xFF;
		}
	}
#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET
#undef A_OFFSET
}

void SaveRGBBin(BYTE* rgbsrc, char* dstfile, UINT32 width, UINT32 height)
{
	FILE* fp = fopen(dstfile, "wb");
	fwrite(rgbsrc, width * height * 3, 1, fp);
	fclose(fp);
}

void SaveRawBayer(BYTE* bayersrc, char* dstfile, UINT32 width, UINT32 height)
{
	FILE* fp = fopen(dstfile, "wb");
	BYTE bSkip[128];
	memset(bSkip, 0, 128);
	UINT32* pSkip = (UINT32*)bSkip;
	pSkip[0] = width * 2;
	pSkip[1] = height;
	fwrite(bSkip, 128, 1, fp);
	fwrite(bayersrc, width * height * 2, 1, fp);
	fclose(fp);
}

void SaveRawBayerASCII(BYTE* bayersrc, char* dstfile, UINT32 width, UINT32 height)
{
	FILE* fp = fopen(dstfile, "wb");
	//fwrite(bayersrc, 1, width * height, fp);
	char pBuf[16];
	for(int i = 0; i < height; i++)
	{
		USHORT* pLine = (USHORT*)GetLine(bayersrc, i, width, height, 2);
		for(int j = 0; j < width; j++)
		{
			sprintf(pBuf, "%d ", pLine[j]);
			fwrite(pBuf, strlen(pBuf), 1, fp);
		}
		sprintf(pBuf, "\r\n");
		fwrite(pBuf, strlen(pBuf), 1, fp);
	}
	fclose(fp);
}

__inline BYTE GetCenterPixel(BYTE r0c0, BYTE r0c1, BYTE r1c0, BYTE r1c1, double offsetx, double offsety)
{
	// offsetx and offsety are required to be in [0.0, 1.0].
	double dr1 = r0c0 * (1 - offsetx) + r0c1 * (offsetx);
	double dr2 = r1c0 * (1 - offsetx) + r1c1 * (offsetx);
	return (BYTE)(dr1 * (1 - offsety) + dr2 * offsety);
}

#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0

void RGBAScale(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight)
{
	// Scale operation uses 2x2 pixel for calculation
	double scalex = (double)width / (double)dstwidth;
	double scaley = (double)height/(double)dstheight;
	for(int i = 0; i < dstheight; i++)
	{
		double expecty = scaley * i;
		int strow = (int)(expecty + 0.1);
		if(strow > height - 1)
			strow = height - 1;
		bool bEndOfRow = (strow == (height - 1));
		expecty -= strow;
		if(expecty < 0)
			expecty = 1e-6;
		BYTE* prow0 = GetLine(rgbasrc, strow, width, height, 4);
		BYTE* prow1 = bEndOfRow ? prow0 : GetLine(rgbasrc, strow + 1, width, height, 4);
		BYTE* pdstrow = GetLine(rgbadst, i, dstwidth, dstheight, 4);
		for(int j = 0; j < dstwidth; j++)
		{
			double expectx = scalex * j;
			int stcol = (int)(expectx + 0.1);
			if(stcol > width - 1)
				stcol = width - 1;
			bool bEndOfLine = (stcol == (width - 1));
			BYTE* pbpixr0c0 = prow0 + stcol * 4;
			BYTE* pbpixr0c1 = bEndOfLine ? pbpixr0c0 : prow0 + stcol * 4 + 4;
			BYTE* pbpixr1c0 = prow1 + stcol * 4;
			BYTE* pbpixr1c1 = bEndOfLine ? pbpixr1c0 : prow1 + stcol * 4 + 4;
			expectx -= stcol;
			if(expectx < 0)
				expectx =1e-6;
			pdstrow[G_OFFSET] = GetCenterPixel(pbpixr0c0[G_OFFSET], pbpixr0c1[G_OFFSET], pbpixr1c0[G_OFFSET], pbpixr1c1[G_OFFSET], expectx, expecty);
			pdstrow[B_OFFSET] = GetCenterPixel(pbpixr0c0[B_OFFSET], pbpixr0c1[B_OFFSET], pbpixr1c0[B_OFFSET], pbpixr1c1[B_OFFSET], expectx, expecty);
			pdstrow[R_OFFSET] = GetCenterPixel(pbpixr0c0[R_OFFSET], pbpixr0c1[R_OFFSET], pbpixr1c0[R_OFFSET], pbpixr1c1[R_OFFSET], expectx, expecty);
			pdstrow += 4;
		}
	}
}

void RGBAZoom(BYTE* rgbasrc, BYTE* rgbadst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight)
{
	// Scale operation uses 2x2 pixel for calculation
	double scalex = (double)width / (double)dstwidth;
	double scaley = (double)height/(double)dstheight;
	for(int i = 0; i < dstheight; i++)
	{
		double expecty = scaley * i;
		int strow = (int)(expecty + 0.1);
		if(strow > height - 1)
			strow = height - 1;
		bool bEndOfRow = strow == (height - 1);
		expecty -= strow;
		if(expecty < 0)
			expecty = 1e-6;
		BYTE* prow0 = GetLine(rgbasrc, strow, width, height, 4);
		//BYTE* prow1 = bEndOfRow ? prow0 : GetLine(rgbasrc, strow + 1, width, height, 4);
		BYTE* pdstrow = GetLine(rgbadst, i, dstwidth, dstheight, 4);
		for(int j = 0; j < dstwidth; j++)
		{
			double expectx = scalex * j;
			int stcol = (int)(expectx + 0.1);
			bool bEndOfLine = stcol == (dstwidth - 1);
			BYTE* pbpixr0c0 = prow0 + stcol * 4;
			//BYTE* pbpixr0c1 = bEndOfLine ? pbpixr0c0 : prow0 + stcol * 4 + 4;
			//BYTE* pbpixr1c0 = prow1 + stcol * 4;
			//BYTE* pbpixr1c1 = bEndOfLine ? pbpixr1c0 : prow1 + stcol * 4 + 4;
			//expectx -= stcol;
			//if(expectx < 0)
			//	expectx =1e-6;
			//pdstrow[G_OFFSET] = GetCenterPixel(pbpixr0c0[G_OFFSET], pbpixr0c1[G_OFFSET], pbpixr1c0[G_OFFSET], pbpixr1c1[G_OFFSET], expectx, expecty);
			//pdstrow[B_OFFSET] = GetCenterPixel(pbpixr0c0[B_OFFSET], pbpixr0c1[B_OFFSET], pbpixr1c0[B_OFFSET], pbpixr1c1[B_OFFSET], expectx, expecty);
			//pdstrow[R_OFFSET] = GetCenterPixel(pbpixr0c0[R_OFFSET], pbpixr0c1[R_OFFSET], pbpixr1c0[R_OFFSET], pbpixr1c1[R_OFFSET], expectx, expecty);
			pdstrow[G_OFFSET] = pbpixr0c0[G_OFFSET];
			pdstrow[B_OFFSET] = pbpixr0c0[B_OFFSET];
			pdstrow[R_OFFSET] = pbpixr0c0[R_OFFSET];
			pdstrow += 4;
		}
	}
}

#pragma pack(push)
#pragma pack(1)

typedef struct tagBITMAPFILEHEADER {
        WORD    bfType;
        UINT    bfSize;
        WORD    bfReserved1;
        WORD    bfReserved2;
        UINT    bfOffBits;
} BITMAPFILEHEADER;

#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)

typedef struct tagBITMAPINFOHEADER{
        UINT       biSize;
        UINT       biWidth;
        UINT       biHeight;
        WORD       biPlanes;
        WORD       biBitCount;
        UINT       biCompression;
        UINT       biSizeImage;
        UINT       biXPelsPerMeter;
        UINT       biYPelsPerMeter;
        UINT       biClrUsed;
        UINT       biClrImportant;
} BITMAPINFOHEADER;

#pragma pack(pop)

UINT32 SaveBMP(BYTE* rgbasrc, BYTE* dstfile, UINT32 width, UINT32 height, UINT32 iscolor)
{
	//printf("Size of WORD: %d\r\nSize of UINT: %d\r\n", sizeof(WORD), sizeof(UINT));
	FILE* fp = fopen((char*)dstfile, "wb");
	if(fp == NULL)
		return FALSE;
	BITMAPFILEHEADER filehdr;
	BITMAPINFOHEADER infohdr;
	memset(&filehdr, 0, sizeof(BITMAPFILEHEADER));
	memset(&infohdr, 0, sizeof(BITMAPINFOHEADER));
	// Calculate padding size of a row
	int padding = (4 - (width * (iscolor ? 3 : 1)) % 4) % 4;
	BYTE bpadding[4];
	// Write headers
	filehdr.bfType = ('M' << 8) + 'B';
	// filehdr.bfSize = 54 + 3 * (width + padding) * height;
	filehdr.bfSize = 54 + ((iscolor ? 3 : 1) * width + padding) * height;
	filehdr.bfOffBits = 54;
	infohdr.biWidth = width;
	infohdr.biHeight = height;
	infohdr.biSize = 40;
	infohdr.biPlanes = 1;
	// infohdr.biBitCount = 24;
	infohdr.biBitCount = iscolor ? 24 : 8;
	if(!iscolor)
		infohdr.biClrUsed = 256;
	//printf("Header size: %d\r\n", sizeof(BITMAPFILEHEADER));
	//printf("Infohdr size: %d\r\n", sizeof(BITMAPINFOHEADER));
	BYTE bpalette[256 * 4];
	for(int i = 0; i < 256; i++)
	{
		bpalette[i * 4 + 0] = i;
		bpalette[i * 4 + 1] = i;
		bpalette[i * 4 + 2] = i;
		bpalette[i * 4 + 3] = 0;
	}
	if (!iscolor)
	{
		filehdr.bfSize += 1024;
		filehdr.bfOffBits += 1024;
	}
	fwrite(&filehdr, sizeof(BITMAPFILEHEADER), 1, fp);
	fwrite(&infohdr, sizeof(BITMAPINFOHEADER), 1, fp);
	if(!iscolor)
		fwrite(bpalette, 256 * 4, 1, fp);
	int bmprowlen = width * (iscolor ? 3 : 1) + padding;
	BYTE* btmp = new BYTE[bmprowlen];
	for(int i = 0; i < height; i++)
	{
		BYTE* rgbarow = GetLine(rgbasrc, height - i - 1, width, height, iscolor ? 4 : 1);
		if(iscolor)
		{
			for(int j = 0; j < width; j++)
			{
				btmp[j * 3 + 0] = rgbarow[j * 4 + B_OFFSET];
				btmp[j * 3 + 1] = rgbarow[j * 4 + G_OFFSET];
				btmp[j * 3 + 2] = rgbarow[j * 4 + R_OFFSET];
			}
			// Write row
			fwrite(btmp, bmprowlen, 1, fp);
		}
		else
		{
			fwrite(rgbarow, width, 1, fp);
			if (padding > 0)
				fwrite(bpadding, padding, 1, fp);
		}
	}
	fclose(fp);
	delete []btmp;
	return TRUE;
}

UINT32 SaveWBayerBMP(BYTE* bayersrc, BYTE* dstfile, UINT32 width, UINT32 height, UINT32 dbits)
{
	width /= 2;
	BYTE* ptmp = new BYTE[width * height];
	for(int i = 0; i < width * height; i++)
	{
		int tmp = ((bayersrc[i * 2 + 0] << 0) | (bayersrc[i * 2 + 1] << 8));
		if(dbits > 8)
			tmp = tmp >> (dbits - 8);
		else if(dbits < 8)
			tmp = tmp << (8 - dbits);
		tmp = tmp > 255 ? 255 : tmp;
	}
	UINT32 ret = SaveBMP(ptmp, dstfile, width, height, FALSE);
	delete []ptmp;
	return ret;
}

UINT32 LoadBMP(BYTE* srcfile, BYTE* rgbadst, UINT32* pwidth, UINT32* pheight)
{
	FILE* fp = fopen((char*)srcfile, "rb");
	if(fp == NULL)
		return FALSE;
	BITMAPFILEHEADER filehdr;
	BITMAPINFOHEADER infohdr;
	*pwidth = 0;
	*pheight = 0;
	fread(&filehdr, sizeof(BITMAPFILEHEADER), 1, fp);
	fread(&infohdr, sizeof(BITMAPINFOHEADER), 1, fp);
	//	Scan hdr for pixel line padding, offset, etc.
	int width = infohdr.biWidth;
	int height = infohdr.biHeight;
	int bitcnt = infohdr.biBitCount;
	bool iscolor = true;
	fseek(fp, filehdr.bfOffBits, SEEK_SET);
	//	When input image is invalid, skip.
	if (bitcnt == 8)
	{
		iscolor = false;
	}
	else if (bitcnt != 24)
	{
		fclose(fp);
		return FALSE;
	}
	int padding = (4 - ((width * (iscolor ? 3 : 1)) % 4)) % 4;
	int bmprowlen = width * (iscolor ? 3 : 1) + padding;
	BYTE* pRGBline = new BYTE[bmprowlen];
	for(int i = 0; i < height; i++)
	{
		//	Read one line and write to the corresponding rgba array.
		BYTE* pDstLine = GetLine(rgbadst, height - 1 - i, width, height, 4);
		fread(pRGBline, bmprowlen, 1, fp);
		if (iscolor)
		{
			for (int j = 0; j < width; j++)
			{
				pDstLine[j * 4 + 0] = pRGBline[j * 3 + 0];
				pDstLine[j * 4 + 1] = pRGBline[j * 3 + 1];
				pDstLine[j * 4 + 2] = pRGBline[j * 3 + 2];
			}
		}
		else
		{
			for (int j = 0; j < width; j++)
			{
				pDstLine[j * 4 + 0] = pRGBline[j];
				pDstLine[j * 4 + 1] = pRGBline[j];
				pDstLine[j * 4 + 2] = pRGBline[j];
			}
		}
	}
	fclose(fp);
	delete []pRGBline;
	*pwidth = width;
	*pheight = height;
	return TRUE;
}

UINT32 LoadVerifyBMP(BYTE* srcfile, BYTE* rgbadst, UINT32 width, UINT32 height)
{
	FILE* fp = fopen((char*)srcfile, "rb");
	if(fp == NULL)
		return FALSE;
	BITMAPFILEHEADER filehdr;
	BITMAPINFOHEADER infohdr;
	fread(&filehdr, sizeof(BITMAPFILEHEADER), 1, fp);
	fread(&infohdr, sizeof(BITMAPINFOHEADER), 1, fp);
	fclose(fp);
	//	Scan hdr for pixel line padding, offset, etc.
	if((width != infohdr.biWidth) || (height != infohdr.biHeight))
		return FALSE;
	//	Load BMP file
	LoadBMP(srcfile, rgbadst, &width, &height);
	return TRUE;
}

void AddVisualBars(BYTE* rgbafrm, UINT32 width, UINT32 height)
{
	//	Add from 1 to 8 rows / columns and each segment has 48 pixels.
	UINT32* puRGBA = (UINT32*)rgbafrm;
	for(int i = 0; i < 16; i++)
	{
		for(int j = 0; j < 48; j++)
		{
			puRGBA[width * (1 + i) + i * 48 + j + 1] = 0x00FFFFFF;
			puRGBA[width * (1 + i) + width - 2 - j - i * 48] = 0x00FFFFFF;
			puRGBA[width * (height - 2 - i) + i * 48 + j + 1] = 0x00FFFFFF;
			puRGBA[width * (height - 2 - i) + width - 2 - j - i * 48] = 0x00FFFFFF;
		}
	}
	for(int i = 0; i < 16; i++)
	{
		for(int j = 0; j < 48; j++)
		{
			puRGBA[width * (j + 1 + i * 48) + i + 1] = 0x00FFFFFF;
			puRGBA[width * (j + 1 + i * 48) + width - 2 - i] = 0x00FFFFFF;
			puRGBA[width * (height - 2 - j - i * 48) + i + 1] = 0x00FFFFFF;
			puRGBA[width * (height - 2 - j - i * 48) + width - 2 - i] = 0x00FFFFFF;
		}
	}
}

#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET


void Bayer2RGBA_Crop(BYTE* bayersrc, UINT32 orgwidth, UINT32 orgheight, BYTE* RGBAdst, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty, UINT32 shiftx, UINT32 shifty)
{
	//printf("Call Bayer2RGB.\r\n");
	Bayer2RGB(bayersrc, RGBAdst, orgwidth, orgheight, 1, 4, dstwidth, dstheight, startx, starty, shiftx, shifty);
	//printf("Bayer2RGB End.\r\n");
}

void Gray2RGBA_Crop(BYTE* graysrc, UINT32 orgwidth, UINT32 orgheight, BYTE* RGBAdst, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty, UINT32 shiftx, UINT32 shifty)
{
	Bayer2RGB(graysrc, RGBAdst, orgwidth, orgheight, 1, 4, dstwidth, dstheight, startx, starty, shiftx, shifty, 0, 1);
}

void Bayer2RGB_Crop(BYTE* bayersrc, UINT32 orgwidth, UINT32 orgheight, BYTE* RGBdst, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty, UINT32 shiftx, UINT32 shifty)
{
	Bayer2RGB(bayersrc, RGBdst, orgwidth, orgheight, 1, 3, dstwidth, dstheight, startx, starty, shiftx, shifty);
}

void Bayer2RGBA(BYTE* bayersrc, BYTE* RGBAdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty)
{
	Bayer2RGB(bayersrc, RGBAdst, width, height, 1, 4, width, height, 0, 0, shiftx, shifty);
}

void Bayer2RGB(BYTE* bayersrc, BYTE* RGBdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty)
{
	Bayer2RGB(bayersrc, RGBdst, width, height, 1, 3, width, height, 0, 0, shiftx, shifty);
}

void Bayer2RGB_Split(BYTE* bayersrc, BYTE* RGBdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty)
{
	Bayer2RGB(bayersrc, RGBdst, width, height, 1, 3, width, height, 0, 0, shiftx, shifty, 1);
}

void Bayer2RGBA_OpenCV(BYTE* bayersrc, BYTE* RGBAdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty)
{
	Bayer2RGB(bayersrc, RGBAdst, width, height, 3, 4, width, height, 0, 0, shiftx, shifty);
}

void Bayer2RGB_OpenCV(BYTE* bayersrc, BYTE* RGBdst, UINT32 width, UINT32 height, UINT32 shiftx, UINT32 shifty)
{
	Bayer2RGB(bayersrc, RGBdst, width, height, 3, 3, width, height, 0, 0, shiftx, shifty);
}

#define R_OFFSET 2
#define G_OFFSET 1
#define B_OFFSET 0

static double WB_R = 0.82;
static double WB_G = 0.66;
static double WB_B = 1.0;

void AutoWBCalibrate(BYTE* rgbasrc, UINT32 width, UINT32 height, double* rgbscale)
{
	for(int i = 0; i < 3; i++)
		rgbscale[i] = 0;
	for(int i = 0; i < height; i++)
	{
		BYTE* prow0 = GetLine(rgbasrc, i, width, height, 4);
		for(int j = 0; j < width; j++)
		{
			rgbscale[B_OFFSET] += prow0[j * 4 + B_OFFSET];
			rgbscale[R_OFFSET] += prow0[j * 4 + R_OFFSET];
			rgbscale[G_OFFSET] += prow0[j * 4 + G_OFFSET];
		}
	}
	for(int i = 0; i < 3; i++)
	{
		rgbscale[i] /= (width * height);
		//	The higher value indicates this color should be reduced.
		rgbscale[i] = 1.0 / rgbscale[i];
	}
	//	Scale to 0 ~ 1
	double maxscale = 0.0;
	for(int i = 0; i < 3; i++)
	{
		if(rgbscale[i] > maxscale)
			maxscale = rgbscale[i];
	}
	for(int i = 0; i < 3; i++)
		rgbscale[i] /= maxscale;
	WB_R = rgbscale[R_OFFSET];
	WB_G = rgbscale[G_OFFSET];
	WB_B = rgbscale[B_OFFSET];
}

void PerformAutoWB(BYTE* rgbasrc, UINT32 width, UINT32 height)
{
	for(int i = 0; i < width * height; i++)
	{
		rgbasrc[i * 4 + B_OFFSET] *= WB_B;
		rgbasrc[i * 4 + G_OFFSET] *= WB_G;
		rgbasrc[i * 4 + R_OFFSET] *= WB_R;
	}
}

#undef R_OFFSET
#undef G_OFFSET
#undef B_OFFSET

void ConvertSaveRawFile(BYTE* srcfile, BYTE* dstfile, UINT32 width, UINT32 height)
{
	FILE* fp = fopen((char*)srcfile, "rb");
	BYTE* fRaw = new BYTE[width * height * 2];
	fread(fRaw, 128, 1, fp);
	fread(fRaw, width * height * 2, 1, fp);
	fclose(fp);
	//	Convert to byte
	for(int i = 0; i < width * height; i++)
	{
		fRaw[i] = (fRaw[i * 2 + 1] << 4) | (fRaw[i * 2] >> 4);
	}
	SaveBMP(fRaw, dstfile, width, height, 0);
	delete []fRaw;
}

void BayerPixelNormalizeTemplateGen(BYTE* bayersrc, USHORT* templ, UINT32 width, UINT32 height)
{
	//	Do translation on different channel separately.
	int r0c0 = 0;
	int r0c1 = 0;
	int r1c0 = 0;
	int r1c1 = 0;
	//	R0C0
	for(int i = 0; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 0; j < width; j += 2)
		{
			//if(bayersrc[offset + j] > r0c0)
			r0c0 += bayersrc[offset + j];
		}
	}
	r0c0 /= width * height / 4;
	for(int i = 0; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 0; j < width; j += 2)
		{
			int c = ((UINT32)bayersrc[offset + j]) * 32768 / r0c0;
			templ[offset + j] = (c > 65535) ? 65535 : c;
		}
	}
	//	R0C1
	for(int i = 0; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 1; j < width; j += 2)
		{
			//if(bayersrc[offset + j] > r0c1)
			r0c1 += bayersrc[offset + j];
		}
	}
	r0c1 /= width * height / 4;
	for(int i = 0; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 1; j < width; j += 2)
		{
			int c = ((UINT32)bayersrc[offset + j]) * 32768 / r0c1;
			templ[offset + j] = (c > 65535) ? 65535 : c;
		}
	}
	//	R1C0
	for(int i = 1; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 0; j < width; j += 2)
		{
			//if(bayersrc[offset + j] > r1c0)
			r1c0 += bayersrc[offset + j];
		}
	}
	r1c0 /= width * height / 4;
	for(int i = 1; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 0; j < width; j += 2)
		{
			int c = ((UINT32)bayersrc[offset + j]) * 32768 / r1c0;
			templ[offset + j] = (c > 65535) ? 65535 : c;
		}
	}
	//	R1C1
	for(int i = 1; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 1; j < width; j += 2)
		{
			//if(bayersrc[offset + j] > r1c1)
			r1c1 += bayersrc[offset + j];
		}
	}
	r1c1 /= width * height / 4;
	for(int i = 1; i < height; i += 2)
	{
		int offset = i * width;
		for(int j = 1; j < width; j += 2)
		{
			int c = ((UINT32)bayersrc[offset + j]) * 32768 / r1c1;
			templ[offset + j] = (c > 65535) ? 65535 : c;
		}
	}
}

void BayerPixelNormalize(BYTE* bayersrc, USHORT* templ, UINT32 width, UINT32 height)
{
	for(int i = 0; i < height; i++)
	{
		int offset = i * width;
		for(int j = 0; j < width; j++)
		{
			templ[offset + j] = (templ[offset + j] == 0) ? 32768 : templ[offset + j];
			int tmp = ((UINT32)bayersrc[offset + j]) * 32768 / templ[offset + j];
			bayersrc[offset + j] = (tmp > 255) ? 255 : tmp;
		}
	}
}

void ROI8(BYTE *bytesrc, BYTE *bytedst, UINT32 width, UINT32 height, UINT32 dstwidth, UINT32 dstheight, UINT32 startx, UINT32 starty)
{
	//	Calculate dstwidth & dstheight padding.
	int widthpad = startx + dstwidth - width;
	int heightpad = starty + dstheight - height;
	if(widthpad < 0)
		widthpad = 0;
	if(heightpad < 0)
		heightpad = 0;
	int effectivewidth = dstwidth - widthpad;
	int effectiveheight = dstheight - heightpad;

	//	Clear all first
	memset(bytedst, 0x00000000, dstwidth * dstheight);
	//	Now copy. Copy to a small area.
	for(int i = 0; i < effectiveheight; i++)
	{
		BYTE *psrc = GetLine(bytesrc, starty + i, width, height, 1);
		BYTE *pdst = GetLine(bytedst, i, dstwidth, dstheight, 1);
		//	Pass startx
		psrc += startx;
		//	Now do copy.
		memcpy(pdst, psrc, effectivewidth);
	}
}

void Map8(BYTE *bytesrc, BYTE *bytedst, UINT32 width, UINT32 height, BYTE *palette)
{
	for(int i = 0; i < width * height; i++)
	{
		bytedst[i] = palette[bytesrc[i]];
	}
}

__inline void Sort3(BYTE *psrc)
{
	BYTE tmp;
	if(psrc[1] > psrc[2])
	{
		tmp = psrc[2];
		psrc[2] = psrc[1];
		psrc[1] = tmp;
	}
	else
	{
		//	Fast exit
		if(psrc[1] >= psrc[0])
			return;
	}
	if(psrc[0] > psrc[2])
	{
		tmp = psrc[2];
		psrc[2] = psrc[0];
		psrc[0] = tmp;
	}
	if(psrc[0] > psrc[1])
	{
		tmp = psrc[1];
		psrc[1] = psrc[0];
		psrc[0] = tmp;
	}
}

__inline BYTE Max3(BYTE psrc0, BYTE psrc1, BYTE psrc2)
{
	BYTE tmp;
	if(psrc0 > psrc1)
		tmp = psrc0;
	else
		tmp = psrc1;
	if(tmp > psrc2)
		return tmp;
	else 
		return psrc2;
}

__inline BYTE Min3(BYTE psrc0, BYTE psrc1, BYTE psrc2)
{
	BYTE tmp;
	if(psrc0 < psrc1)
		tmp = psrc0;
	else
		tmp = psrc1;
	if(tmp < psrc2)
		return tmp;
	else 
		return psrc2;
}

__inline BYTE Med3(BYTE psrc0, BYTE psrc1, BYTE psrc2)
{
	BYTE larger, smaller;
	if(psrc0 > psrc1)
	{
		larger = psrc0;
		smaller = psrc1;
	}
	else
	{
		larger = psrc1;
		smaller = psrc0;
	}
	if(psrc2 >= larger)
		return larger;
	else if(psrc2 <= smaller)
		return smaller;
	else
		return psrc2;
}

__inline void Replace3(BYTE *psrc, BYTE src, BYTE dst)
{
	if(psrc[0] == src)
		psrc[0] = dst;
	else if(psrc[1] == src)
		psrc[1] = dst;
	else
		psrc[2] = dst;
}

void MedianFilter8(BYTE *bytesrc, BYTE *bytedst, UINT32 width, UINT32 height)
{
	BYTE bRow0[3], bRow1[3], bRow2[3];

	//	Do following calculation:
	//	1. Sort each row;
	//	2. Find the min / max value and find the median value.

	for(int i = 0; i < height - 2; i++)
	{
		BYTE *prow0 = GetLine(bytesrc, i + 0, width, height, 1);
		BYTE *prow1 = GetLine(bytesrc, i + 1, width, height, 1);
		BYTE *prow2 = GetLine(bytesrc, i + 2, width, height, 1);
		BYTE *pdst1 = GetLine(bytedst, i + 1, width, height, 1);
		pdst1++;

		for(int j = 0; j < width - 2; j++)
		{
			//	Replace prowx[j - 1] with prowx[j + 2].
			for(int k = 0; k < 3; k++)
			{
				bRow0[k] = prow0[j + k];
				bRow1[k] = prow1[j + k];
				bRow2[k] = prow2[j + k];
			}
			//Replace3(bRow0, prow0[j - 1], prow0[j + 2]);
			//Replace3(bRow1, prow1[j - 1], prow1[j + 2]);
			//Replace3(bRow2, prow2[j - 1], prow2[j + 2]);
			Sort3(bRow0);
			Sort3(bRow1);
			Sort3(bRow2);
			pdst1[j] = Med3(Max3(bRow0[0], bRow1[0], bRow2[0]), Med3(bRow0[1], bRow1[1], bRow2[1]), Min3(bRow0[2], bRow1[2], bRow2[2]));
		}
	}
}
