#ifndef __RAW_STREAM_PARSER
#define __RAW_STREAM_PARSER

#include "CommonTypes.h"

#ifdef __cplusplus
extern "C"{
#endif

void* InitializeStreamParser();

BYTE* ParseStream(void* parser, BYTE* pbuffer, UINT32 length, UINT32* pwidth, UINT32* pheight);

void DestroyStreamParser(void* parser);

#ifdef __cplusplus
}
#endif

#endif