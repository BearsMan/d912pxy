/*
MIT License

Copyright(c) 2018-2019 megai2

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#include "stdafx.h"

template<class ElementType, class ProcImpl>
d912pxy_pool_memcat<ElementType, ProcImpl>::d912pxy_pool_memcat() : d912pxy_pool<ElementType, ProcImpl>()
{

}

template<class ElementType, class ProcImpl>
d912pxy_pool_memcat<ElementType, ProcImpl>::~d912pxy_pool_memcat()
{
	if (memPool)
		memPool->Release();

	delete memPoolLock;

	PXY_FREE(memTable);
	PXY_FREE(this->rwMutex);
}

template<class ElementType, class ProcImpl>
void d912pxy_pool_memcat<ElementType, ProcImpl>::Init(UINT32 iBitIgnore, UINT32 iBitLimit, d912pxy_config_value limitCfg)
{
	d912pxy_pool<ElementType, ProcImpl>::Init();

	memPoolLock = new d912pxy_thread_lock();

	memPool = NULL;

	if (memPoolSize)
		CreateMemPool();

	bitIgnore = iBitIgnore;
	bitLimit = iBitLimit;
	bitCnt = iBitLimit - iBitIgnore;

	maxMemoryInPool = 0;
	memoryInPool = 0;

	if (limitCfg != PXY_CFG_CNT)	
		maxMemoryInPool = d912pxy_s.config.GetValueUI32(limitCfg) << 20;			

	PXY_MALLOC(memTable, sizeof(void*)*bitCnt, d912pxy_ringbuffer<ElementType>**);
	PXY_MALLOC(this->rwMutex, sizeof(d912pxy_thread_lock)*bitCnt, d912pxy_thread_lock*);

	for (int i = 0; i != bitCnt; ++i)
	{
		memTable[i] = new d912pxy_ringbuffer<ElementType>(64, 2);
		this->rwMutex[i].Init();
	}
}

template<class ElementType, class ProcImpl>
d912pxy_ringbuffer<ElementType>* d912pxy_pool_memcat<ElementType, ProcImpl>::GetCatBuffer(UINT32 cat)
{
	return memTable[cat];
}

template<class ElementType, class ProcImpl>
void d912pxy_pool_memcat<ElementType, ProcImpl>::PoolUnloadProc(ElementType val, UINT32 cat)
{
	if (!IsPoolHaveFreeSpace())
	{		
		if (val->IsPersistentlyPooled())
			return;

		val->NoteDeletion(GetTickCount());
		d912pxy_s.thread.cleanup.Watch(val);
	}
	else {
		val->PoolPersistently();
	}
}

template<class ElementType, class ProcImpl>
UINT d912pxy_pool_memcat<ElementType, ProcImpl>::MemCatFromSize(UINT sz)
{
	UINT64 iv = (sz >> bitIgnore) + 1 * ((sz & ((1 << (bitIgnore)) - 1)) != 0);
	if (!iv)
		return 0;
	UINT ret = 0;
	while (iv > (1ULL << ret))
	{
		++ret;
	}
	
	return ret;
}

template<class ElementType, class ProcImpl>
UINT d912pxy_pool_memcat<ElementType, ProcImpl>::MemCatToSize(UINT cat)
{
	return 1 << (cat + bitIgnore);
}

template<class ElementType, class ProcImpl>
UINT d912pxy_pool_memcat<ElementType, ProcImpl>::IsPoolHaveFreeSpace()
{
	return maxMemoryInPool > memoryInPool;
}

template<class ElementType, class ProcImpl>
void d912pxy_pool_memcat<ElementType, ProcImpl>::AddMemoryToPool(INT sz)
{
	memoryInPool += sz;
}

template<class ElementType, class ProcImpl>
UINT32 d912pxy_pool_memcat<ElementType, ProcImpl>::GetMemoryInPoolMb()
{
	return memoryInPool >> 20;
}

template<class ElementType, class ProcImpl>
void d912pxy_pool_memcat<ElementType, ProcImpl>::CreateMemPool()
{
	if (memPool)
		memPool->Release();

	memPoolOffset = 0;

	const D3D12_HEAP_DESC heapDsc = {
		memPoolSize,
		d912pxy_s.dev.GetResourceHeap(memPoolHeapType),
		0,
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS
	};

	d912pxy_s.dx12.dev->CreateHeap(
		&heapDsc,
		IID_PPV_ARGS(&memPool)
	);
}

template<class ElementType, class ProcImpl>
ID3D12Resource * d912pxy_pool_memcat<ElementType, ProcImpl>::CreatePlacedResource(UINT32 size, D3D12_RESOURCE_DESC* rsDesc)
{
	ID3D12Resource * ret = NULL;

	memPoolLock->Hold();

	if (memPoolOffset + size >= memPoolSize)
	{
		CreateMemPool();

		memPoolOffset = 0;
	}

	HRESULT cprHR = d912pxy_s.dx12.dev->CreatePlacedResource(
		memPool,
		memPoolOffset,
		rsDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&ret)
	);

	if (FAILED(cprHR))
	{		
		memPoolLock->Release();
		return NULL;
	}

	memPoolOffset += size;

	memPoolLock->Release();

	return ret;
}

template class d912pxy_pool_memcat<d912pxy_vstream*, d912pxy_vstream_pool*>;
template class d912pxy_pool_memcat<d912pxy_upload_item*, d912pxy_upload_pool*>;