#include "stdafx.h"

#define API_OVERHEAD_TRACK_LOCAL_ID_DEFINE PXY_METRICS_API_OVERHEAD_DEVICE_DRAWING_UP

d912pxy_draw_up::d912pxy_draw_up(d912pxy_device* dev) : d912pxy_noncom(L"draw_up")
{
	for (int i = 0; i != PXY_DUP_COUNT; ++i)
	{
		UINT32 tmpUPbufSpace = 0xFFFF;
		if (i == PXY_DUP_DPI)
		{
			tmpUPbufSpace = d912pxy_s.config.GetValueXI64(PXY_CFG_MISC_DRAW_UP_BUFFER_LENGTH) & 0xFFFFFFFF;
		} 

		AllocateBuffer((d912pxy_draw_up_buffer_name)i, tmpUPbufSpace);
		LockBuffer((d912pxy_draw_up_buffer_name)i);

		if (i == PXY_DUP_DPI)
		{
			for (int j = 0; j != tmpUPbufSpace >> 2; ++j)
			{
				((UINT32*)buf[i].writePoint)[j] = j;
			}

			buf[i].offset = tmpUPbufSpace;
		}
	}
}


d912pxy_draw_up::~d912pxy_draw_up()
{
	OnFrameEnd();

	for (int i = 0; i != PXY_DUP_COUNT; ++i)
	{
		buf[i].vstream->Release();
	}
}

void d912pxy_draw_up::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void * pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	

	LOG_DBG_DTDM2("DPUP %u %u %016llX %u", PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);

	UINT vstreamRegLen = VertexStreamZeroStride * d912pxy_s.render.iframe.GetIndexCount(PrimitiveCount, PrimitiveType);
	UINT vsvOffset = BufferWrite(PXY_DUP_DPV, vstreamRegLen, pVertexStreamZeroData);

	PushVSBinds();

	d912pxy_s.render.iframe.SetIBuf(buf[PXY_DUP_DPI].vstream);
	d912pxy_s.render.iframe.SetVBuf(buf[PXY_DUP_DPV].vstream, 0, vsvOffset, VertexStreamZeroStride);

	//megai2: FIXME forced CommitBatch2 here for now, need to properly reflect config file
	d912pxy_s.render.iframe.CommitBatch2(PrimitiveType, 0, 0, 0, 0, PrimitiveCount);	

	PopVSBinds();

	
}

void d912pxy_draw_up::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void * pIndexData, D3DFORMAT IndexDataFormat, const void * pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	

	UINT hiInd = IndexDataFormat == D3DFMT_INDEX32;
	d912pxy_draw_up_buffer_name indBuf = hiInd ? PXY_DUP_DIPI4 : PXY_DUP_DIPI2;

	UINT indBufSz = (hiInd * 2 + 2)*d912pxy_s.render.iframe.GetIndexCount(PrimitiveCount, PrimitiveType);
	UINT vertBufSz = VertexStreamZeroStride * (MinVertexIndex + NumVertices);

	UINT vsiOffset = BufferWrite(indBuf, indBufSz, pIndexData);
	UINT vsvOffset = BufferWrite(PXY_DUP_DIPV, vertBufSz, pVertexStreamZeroData);

	PushVSBinds();

	d912pxy_s.render.iframe.SetIBuf(buf[indBuf].vstream);
	d912pxy_s.render.iframe.SetVBuf(buf[PXY_DUP_DIPV].vstream, 0, vsvOffset, VertexStreamZeroStride);	
	//megai2: FIXME forced CommitBatch2 here for now, need to properly reflect config file
	d912pxy_s.render.iframe.CommitBatch2(PrimitiveType, 0, MinVertexIndex, NumVertices, vsiOffset >> (1 + hiInd), PrimitiveCount);		

	PopVSBinds();

	
}

void d912pxy_draw_up::OnFrameEnd()
{
	for (int i = 0; i != PXY_DUP_COUNT; ++i)
	{
		if (buf[i].writePoint)
			buf[i].vstream->UnlockRanged(0, buf[i].offset);

		buf[i].writePoint = 0;
	}
}

UINT d912pxy_draw_up::BufferWrite(d912pxy_draw_up_buffer_name bid, UINT size, const void * src)
{
	if (!buf[bid].writePoint)
		LockBuffer(bid);

	intptr_t tmpWP = buf[bid].writePoint;
	if ((tmpWP + size) >= buf[bid].endPoint)
	{
		UINT32 len = buf[bid].vstream->GetLength();
		len = (len * 2) & 0x7FFFFFF;

		buf[bid].vstream->Unlock();		
		buf[bid].vstream->Release();

		AllocateBuffer(bid, len);
		LockBuffer(bid);

		tmpWP = buf[bid].writePoint;

		if ((tmpWP + size) >= buf[bid].endPoint)
		{
			LOG_DBG_DTDM3("App asked %lX ibytes to draw, TOO BIG, skipping", size);
			return 0;
		}
	} 

	memcpy((void*)tmpWP, src, size);

	buf[bid].writePoint += size;

	UINT ret = buf[bid].offset;
	buf[bid].offset += size;

	return ret;
}

void d912pxy_draw_up::LockBuffer(d912pxy_draw_up_buffer_name bid)
{
	buf[bid].vstream->Lock(0, 0, (void**)&buf[bid].writePoint, 0);
	buf[bid].endPoint = buf[bid].writePoint + buf[bid].vstream->GetLength();
	buf[bid].offset = 0;
}

void d912pxy_draw_up::AllocateBuffer(d912pxy_draw_up_buffer_name bid, UINT len)
{
	UINT nFmt;
	UINT nIB;

	switch (bid)
	{
	case PXY_DUP_DPV:
	case PXY_DUP_DIPV:
		nFmt = 0;
		nIB = 0;
		break;
	case PXY_DUP_DPI:
	case PXY_DUP_DIPI4:
		nFmt = D3DFMT_INDEX32;
		nIB = 1;
		break;
	case PXY_DUP_DIPI2:
		nFmt = D3DFMT_INDEX16;
		nIB = 1;
	}

	buf[bid].vstream = d912pxy_s.pool.vstream.GetVStreamObject(len, nFmt, nIB);
}

void d912pxy_draw_up::PushVSBinds()
{
	oi = d912pxy_s.render.iframe.GetIBuf();
	oss = d912pxy_s.render.iframe.GetStreamSource(0);
	ossi = d912pxy_s.render.iframe.GetStreamSource(1);

	d912pxy_s.render.iframe.SetStreamFreq(0, 1);
	d912pxy_s.render.iframe.SetStreamFreq(1, 0);
}

void d912pxy_draw_up::PopVSBinds()
{
	d912pxy_s.render.iframe.SetIBuf(oi);
	d912pxy_s.render.iframe.SetVBuf(oss.buffer, 0, oss.offset, oss.stride);
	d912pxy_s.render.iframe.SetStreamFreq(0, oss.divider);
	d912pxy_s.render.iframe.SetStreamFreq(1, ossi.divider);
}

#undef API_OVERHEAD_TRACK_LOCAL_ID_DEFINE 