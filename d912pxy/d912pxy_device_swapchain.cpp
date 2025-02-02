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

#define API_OVERHEAD_TRACK_LOCAL_ID_DEFINE PXY_METRICS_API_OVERHEAD_DEVICE_SWAPCHAIN

HRESULT d912pxy_device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain)
{ 
	LOG_DBG_DTDM(__FUNCTION__);
	//zero is always present
	for (int i = 1; i != PXY_INNER_MAX_SWAP_CHAINS; ++i)
	{
		if (swapchains[i])
			continue;
		
		swapchains[i] = &d912pxy_swapchain::d912pxy_swapchain_com(i, pPresentationParameters)->swapchain;

		*pSwapChain = PXY_COM_CAST_(IDirect3DSwapChain9, swapchains[i]);

		return D3D_OK;
	}
	return D3DERR_OUTOFVIDEOMEMORY;
}

HRESULT d912pxy_device::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain)
{ 
	LOG_DBG_DTDM(__FUNCTION__);
	if (iSwapChain >= PXY_INNER_MAX_SWAP_CHAINS)
		return D3DERR_INVALIDCALL;

	*pSwapChain = PXY_COM_CAST_(IDirect3DSwapChain9, swapchains[iSwapChain]);

	return D3D_OK; 
}

UINT d912pxy_device::GetNumberOfSwapChains(void)
{ 
	LOG_DBG_DTDM(__FUNCTION__);
	//This method returns the number of swap chains created by CreateDevice.
	return 1; 
}

HRESULT d912pxy_device::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{ 
	LOG_DBG_DTDM(__FUNCTION__);

	

	swapOpLock.Hold();

	m_dupEmul->OnFrameEnd();
	d912pxy_s.render.iframe.End();
	d912pxy_s.dx12.que.Flush(0);

	HRESULT ret = swapchains[0]->SetPresentParameters(pPresentationParameters);
	
	d912pxy_s.render.iframe.Start();

	swapOpLock.Release();

	
		
	return ret; 
}

HRESULT d912pxy_device::InnerPresentExecute()
{
	LOG_DBG_DTDM(__FUNCTION__);

	

	swapOpLock.Hold();

	m_dupEmul->OnFrameEnd();
	d912pxy_s.render.iframe.End();

	
	FRAME_METRIC_PRESENT(0)

	LOG_DBG_DTDM2("Present Exec GPU");
	return d912pxy_s.dx12.que.ExecuteCommands(1);
}

void d912pxy_device::InnerPresentFinish()
{
	LOG_DBG_DTDM(__FUNCTION__);

	FRAME_METRIC_PRESENT(1)
	

	d912pxy_s.render.iframe.Start();

	swapOpLock.Release();

	LOG_DBG_DTDM("Present finished");

	
}

HRESULT d912pxy_device::Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{ 
	HRESULT ret = InnerPresentExecute();

#ifdef ENABLE_METRICS
	d912pxy_s.log.metrics.TrackDrawCount(d912pxy_s.render.iframe.GetBatchCount());
	d912pxy_s.log.metrics.FlushIFrameValues();	
#endif 
	
	InnerPresentFinish();

	return ret;
}

HRESULT d912pxy_device::Present_PG(const RECT * pSourceRect, const RECT * pDestRect, HWND hDestWindowOverride, const RGNDATA * pDirtyRegion)
{
	HRESULT ret = InnerPresentExecute();

	perfGraph->RecordPresent(d912pxy_s.render.iframe.GetBatchCount());

#ifdef ENABLE_METRICS
	d912pxy_s.log.metrics.TrackDrawCount(d912pxy_s.render.iframe.GetBatchCount());
	d912pxy_s.log.metrics.FlushIFrameValues();
#endif s

	InnerPresentFinish();

	return ret;
}

HRESULT d912pxy_device::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer)
{
	LOG_DBG_DTDM(__FUNCTION__);

	

	HRESULT ret = swapchains[iSwapChain]->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);

	

	return ret;
}

HRESULT d912pxy_device::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus)
{
	LOG_DBG_DTDM(__FUNCTION__);

	return PXY_COM_CAST_(IDirect3DSwapChain9, swapchains[iSwapChain])->GetRasterStatus(pRasterStatus);
}

void d912pxy_device::SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp)
{ 
	LOG_DBG_DTDM(__FUNCTION__);

	

	swapchains[iSwapChain]->SetGammaRamp(Flags, pRamp);

	
}

void d912pxy_device::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp)
{ 
	LOG_DBG_DTDM(__FUNCTION__);

	

	swapchains[iSwapChain]->GetGammaRamp(pRamp);

	
}

HRESULT d912pxy_device::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) 
{	
	LOG_DBG_DTDM(__FUNCTION__);

	

	swapchains[iSwapChain]->GetFrontBufferData(pDestSurface);

	

	return D3D_OK;
}

HRESULT d912pxy_device::TestCooperativeLevel(void)
{
	LOG_DBG_DTDM(__FUNCTION__);

	

	swapOpLock.Hold();

	HRESULT ret = swapchains[0]->TestCoopLevel();

	swapOpLock.Release();

	

	return ret;
}

#undef API_OVERHEAD_TRACK_LOCAL_ID_DEFINE 