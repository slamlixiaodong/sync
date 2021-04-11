#include "RawStreamParser.h"

#include "FrameSync.h"

void* InitializeStreamParser()
{
	FrameSync* sync = new FrameSync();
	return sync;
}

BYTE* ParseStream(void* parser, BYTE* pbuffer, UINT32 length, UINT32* pwidth, UINT32* pheight)
{
	FrameSync* sync = (FrameSync*)parser;
	return sync->PutBuffer(pbuffer, FALSE, length, (int*)pwidth, (int*)pheight);
}

void DestroyStreamParser(void* parser)
{
	FrameSync* sync = (FrameSync*)parser;
	delete sync;
}
