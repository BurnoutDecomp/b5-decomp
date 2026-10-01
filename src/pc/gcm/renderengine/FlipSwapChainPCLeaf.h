#pragma once

#include <d3d9.h>
#include "pc/gcm/renderengine/GpuFrameTimingPCLeaf.h"
#include "pc/gcm/renderengine/GeometryBindingsPCLeaf.h"
#include "pc/gcm/renderengine/ShaderBindingsPCLeaf.h"

// FLAG PC-platform leaf: primary D3D9Ex flip output. ResetEx must execute on the
// device-creation thread, between joined engine frames. A failed reset is rolled
// back before drawing; if rollback also fails, the main loop waits for recovery.
namespace renderengine
{
    template<class DeviceType> class BasicFlipSwapChainPC
    {
        DeviceType* mpDevice = nullptr;
        D3DPRESENT_PARAMETERS mParameters = {};
        DWORD muThread = 0;
        UINT muFailedWidth = 0, muFailedHeight = 0;
        unsigned muGeneration = 0;
        bool mbLost = false;
    public:
        BasicFlipSwapChainPC() = default;
        BasicFlipSwapChainPC(const BasicFlipSwapChainPC&) = delete;
        BasicFlipSwapChainPC& operator=(const BasicFlipSwapChainPC&) = delete;
        ~BasicFlipSwapChainPC() { Release(); }
        void Release()
        {
            if(mpDevice)mpDevice->Release();
            mpDevice=nullptr; mParameters={}; muThread=0;
            muFailedWidth=muFailedHeight=muGeneration=0; mbLost=false;
        }
        void Configure(DeviceType* lpDevice,const D3DPRESENT_PARAMETERS& lrParameters)
        {
            Release(); mpDevice=lpDevice; mpDevice->AddRef();
            mParameters=lrParameters; muThread=GetCurrentThreadId();
        }
        bool Active() const { return mpDevice!=nullptr; }
        bool Ready() const { return !mbLost; }
        UINT Width() const { return mParameters.BackBufferWidth; }
        UINT Height() const { return mParameters.BackBufferHeight; }
        unsigned Generation() const { return muGeneration; }
        HRESULT Prepare(HWND lhWindow,bool lbVSync,UINT luWidth,UINT luHeight,bool lbAllowReset=true)
        {
            if(!mpDevice || !luWidth || !luHeight || lhWindow!=mParameters.hDeviceWindow)return D3DERR_INVALIDCALL;
            const UINT luInterval=lbVSync?D3DPRESENT_INTERVAL_DEFAULT:D3DPRESENT_INTERVAL_IMMEDIATE;
            if(!mbLost && luWidth==Width() && luHeight==Height() && luInterval==mParameters.PresentationInterval)return S_OK;
            if(!mbLost && luWidth==muFailedWidth && luHeight==muFailedHeight)return E_OUTOFMEMORY;
            if(!lbAllowReset || GetCurrentThreadId()!=muThread)return E_PENDING;
            GpuFrameTimingPC::DeviceReset();
            GeometryBindingsPC::gCache.Invalidate();
            gPCShaderBindingCache.Invalidate();
            D3DPRESENT_PARAMETERS lDesired=mParameters;
            lDesired.BackBufferWidth=luWidth;lDesired.BackBufferHeight=luHeight;lDesired.PresentationInterval=luInterval;
            D3DPRESENT_PARAMETERS lRequest=lDesired; // ResetEx overwrites size/count fields.
            const HRESULT lResult=mpDevice->ResetEx(&lRequest,nullptr);
            ++muGeneration;
            if(SUCCEEDED(lResult))
            {
                mParameters=lDesired;mbLost=false;muFailedWidth=muFailedHeight=0;
                return S_OK;
            }
            mbLost=true;
            lRequest=mParameters;
            if(SUCCEEDED(mpDevice->ResetEx(&lRequest,nullptr)))
            {
                mbLost=false;muFailedWidth=luWidth;muFailedHeight=luHeight;
            }
            return lResult;
        }
        HRESULT GetBackBuffer(IDirect3DSurface9** lppSurface)
        { return mpDevice && !mbLost?mpDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,lppSurface):D3DERR_DEVICELOST; }
        HRESULT Present()
        {
            if(!mpDevice || mbLost)return D3DERR_DEVICELOST;
            const HRESULT lResult=mpDevice->PresentEx(nullptr,nullptr,nullptr,nullptr,0);
            if(lResult==D3DERR_DEVICELOST || lResult==D3DERR_DEVICEHUNG || lResult==D3DERR_DEVICEREMOVED || lResult==S_PRESENT_MODE_CHANGED)mbLost=true;
            return lResult;
        }
    };
    using FlipSwapChainPC=BasicFlipSwapChainPC<IDirect3DDevice9Ex>;
}
