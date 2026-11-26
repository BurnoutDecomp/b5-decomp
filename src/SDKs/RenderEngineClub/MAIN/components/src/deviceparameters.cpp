// XDK size optimization: ARTIST compiled the renderengine core (0x82B60000..0x82B64xxx) for
// size (record-form cr0 tests), like the state objects' resource descriptors.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <cstring>

// ARTIST 82B61984 and 82B61A28 call these XDK CRT providers.
#pragma function(memset, memcpy)

namespace renderengine
{
    // ARTIST82B61960: the output is the XDK's 124-byte presentation record.
    void Device::InitPresentParameters(D3DPRESENT_PARAMETERS& lrPresent, const Parameters& lrParameters)
    {
        memset(&lrPresent, 0, sizeof(lrPresent));
        lrPresent.SwapEffect = lrParameters.swapEffect;
        lrPresent.EnableAutoDepthStencil = FALSE;
        lrPresent.BackBufferWidth = lrParameters.backBufferWidth;
        lrPresent.BackBufferHeight = lrParameters.backBufferHeight;
        lrPresent.BackBufferCount = 1;
        lrPresent.BackBufferFormat = static_cast<D3DFORMAT>(lrParameters.backBufferFormat);
        lrPresent.AutoDepthStencilFormat = static_cast<D3DFORMAT>(lrParameters.depthStencilFormat);
        lrPresent.MultiSampleType = lrParameters.multiSampleType;
        lrPresent.FullScreen_RefreshRateInHz = lrParameters.refreshRate;
        lrPresent.Flags = lrParameters.flags;
        lrPresent.DisableAutoBackBuffer = TRUE;
        lrPresent.DisableAutoFrontBuffer = TRUE;
        lrPresent.PresentationInterval = lrParameters.numVsyncs;
        lrPresent.FrontBufferFormat = static_cast<D3DFORMAT>(lrParameters.frontBufferFormat);
        lrPresent.RingBufferParameters.Flags = 0;
        lrPresent.FrontBufferColorSpace = lrParameters.frontBufferColorSpace;
        lrPresent.RingBufferParameters.pPrimary = 0;
        lrPresent.RingBufferParameters.PrimarySize = lrParameters.primaryRingBufferSize;
        lrPresent.RingBufferParameters.pSecondary = 0;
        lrPresent.RingBufferParameters.SecondarySize = lrParameters.secondaryRingBufferSize;
        lrPresent.RingBufferParameters.SegmentCount = lrParameters.ringBufferSegmentCount;
        memcpy(&lrPresent.VideoScalerParameters, &lrParameters.videoScalerParameters,
            sizeof(lrPresent.VideoScalerParameters));
    }

    // ARTIST82B616B0: XDK formats accepted for each native surface.
    bool Device::Parameters::Validate() const
    {
        const D3DFORMAT front = static_cast<D3DFORMAT>(frontBufferFormat);
        D3DFORMAT back;
        if (front != D3DFMT_LE_X8R8G8B8 &&
            front != D3DFMT_LE_A8R8G8B8 &&
            front != D3DFMT_LE_X2R10G10B10 &&
            front != D3DFMT_LE_A2R10G10B10)
            goto unsupported;

        back = static_cast<D3DFORMAT>(backBufferFormat);
        if (back != D3DFMT_A8R8G8B8 &&
            back != D3DFMT_X8R8G8B8 &&
            back != D3DFMT_A2R10G10B10 &&
            back != D3DFMT_X2R10G10B10 &&
            back != D3DFMT_A2B10G10R10F_EDRAM &&
            back != D3DFMT_G16R16 &&
            back != D3DFMT_G16R16F &&
            back != D3DFMT_G16R16_EDRAM &&
            back != D3DFMT_A16B16G16R16_EDRAM &&
            back != D3DFMT_R32F &&
            back != D3DFMT_A16B16G16R16 &&
            back != D3DFMT_A16B16G16R16F &&
            back != D3DFMT_G32R32F)
            goto unsupported;

        if (frontBufferWidth < 320)
            goto unsupported;
        if (frontBufferHeight >= 200)
            return true;

    unsupported:
        return false;
    }

    // ARTIST82B617F0: defaults are applied only to the device-owned copy.
    void Device::Parameters::FixD3DDefaults()
    {
        if (!primaryRingBufferSize) primaryRingBufferSize = 0x8000;
        if (!secondaryRingBufferSize) secondaryRingBufferSize = 0x200000;
        if (!ringBufferSegmentCount)
        {
            const u32 luSecondaryRingBufferSize = secondaryRingBufferSize;
            ringBufferSegmentCount = 32;
            if (luSecondaryRingBufferSize > 0x1000000)
            {
                ringBufferSegmentCount = (luSecondaryRingBufferSize * 2) & 0xFFFFFC00;
            }
        }
    }

    // ARTIST 82B61540..82B616A8. Unknown modes deliberately preserve the
    // previous dimensions/refresh, and the bool's three padding bytes survive.
    void Device::Parameters::Initialize(u32 videoMode)
    {
        backBufferFormat = static_cast<PixelFormat>(D3DFMT_A8R8G8B8);
        frontBufferFormat = static_cast<PixelFormat>(D3DFMT_LE_X8R8G8B8);
        multiSampleType = D3DMULTISAMPLE_NONE;
        behaviorFlags = D3DCREATE_BUFFER_2_FRAMES;
        swapEffect = D3DSWAPEFFECT_DISCARD;
        depthStencilFormat = static_cast<PixelFormat>(D3DFMT_D24S8);
        depthStencilAutoCreate = true;
        numVsyncs = 1;
        flags = 0;
        frontBufferColorSpace = D3DCOLORSPACE_RGB;
        primaryRingBufferSize = 0;
        secondaryRingBufferSize = 0;
        ringBufferSegmentCount = 0;

        switch (videoMode)
        {
        case 0:
        case 2:
            backBufferWidth = frontBufferWidth = 640;
            backBufferHeight = frontBufferHeight = 480;
            refreshRate = 60;
            break;
        case 1:
            backBufferWidth = frontBufferWidth = 640;
            backBufferHeight = frontBufferHeight = 576;
            refreshRate = 50;
            break;
        case 3:
            backBufferWidth = frontBufferWidth = 720;
            backBufferHeight = frontBufferHeight = 480;
            refreshRate = 60;
            break;
        case 4:
            backBufferWidth = frontBufferWidth = 1280;
            backBufferHeight = frontBufferHeight = 720;
            refreshRate = 60;
            break;
        case 5:
            backBufferWidth = frontBufferWidth = 1920;
            backBufferHeight = frontBufferHeight = 1080;
            refreshRate = 60;
            break;
        case 6:
            {
                XVIDEO_MODE lVideoMode;
                CGS_STATIC_ASSERT((sizeof(XVIDEO_MODE) == 48), "ARTIST native video mode");
                XGetVideoMode(&lVideoMode);
                backBufferWidth = frontBufferWidth = lVideoMode.dwDisplayWidth;
                backBufferHeight = frontBufferHeight = lVideoMode.dwDisplayHeight;
                refreshRate = 60;
            }
            break;
        }

        videoScalerParameters.ScalerSourceRect.x1 = 0;
        videoScalerParameters.ScalerSourceRect.y1 = 0;
        videoScalerParameters.ScalerSourceRect.x2 = 0;
        videoScalerParameters.ScalerSourceRect.y2 = 0;
        videoScalerParameters.ScaledOutputWidth = 0;
        videoScalerParameters.ScaledOutputHeight = 0;
        videoScalerParameters.FilterProfile = 0;
    }

}
