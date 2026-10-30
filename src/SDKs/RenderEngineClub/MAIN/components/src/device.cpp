#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rendertargetstate.h"
#include "rw/rwcore_structs.h"

D3DDevice* gpD3DDevice = 0;

namespace renderengine
{
    Device::State Device::m_currentState = Device::STATE_COLD;
    Device::Parameters Device::m_parameterCopy;
    u32 gDeviceCounter832716A8;
    u32 gDeviceCounter832716AC;
    LARGE_INTEGER gDevicePerformanceCounter832716B0;
    VertexProgramState* gpCurrentVertexProgramState = 0;
    __declspec(align(16)) PixelBuffer gOriginalColorBuffer;
    __declspec(align(16)) PixelBuffer gOriginalDepthStencilBuffer;
    u32 guTilingEnabled = 0;
    u32 gDeviceFrameCounter83271624 = 0;
    PixelBuffer* gpDeviceColorBuffer = 0;
    PixelBuffer* gpDeviceDepthBuffer = 0;

    // ARTIST82B63BA8. InitializeGlobalBuffers owns the real surface/texture setup.
    bool Device::Start(const Parameters& lrParameters)
    {
        if (!lrParameters.Validate()) return false;
        m_parameterCopy = lrParameters;
        m_parameterCopy.FixD3DDefaults();
        D3DPRESENT_PARAMETERS lPresent;
        InitPresentParameters(lPresent, lrParameters);
        if (Direct3D_CreateDevice(0, D3DDEVTYPE_HAL, 0, lrParameters.behaviorFlags, &lPresent, &gpD3DDevice) != 0)
            return false;
        gDeviceCounter832716A8 = 0;
        gDeviceCounter832716AC = 0;
        InitializeGlobalBuffers();
        D3DDevice_SetVertexDeclaration(gpD3DDevice, 0);
        RenderTargetState::Parameters lTargetParameters = { { gpDeviceColorBuffer, 0, 0, 0, gpDeviceDepthBuffer } };
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = 0x18;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        rw::Resource lResource = rw::ResourceAllocatorRegistry::GetDefaultAllocator()->DoAllocate(lDescriptor, 0);
        m_renderTargetState = RenderTargetState::Initialize(lResource, lTargetParameters);
        QueryPerformanceCounter(&gDevicePerformanceCounter832716B0);
        gpCurrentVertexProgramState = 0;
        m_currentState = STATE_START;
        return true;
    }

    extern u32 gDeviceTileCount83271620;
    extern D3DRECT gDeviceTileRects83271628[8];

    // ARTIST82B61E18: begin the native tile pass or clear selected colour targets.
    void Device::Clear(const ClearColorParameters& lrColor, const ClearDepthStencilParameters& lrDepthStencil,
                       RenderTargetState::TargetID leTarget)
    {
        if (gDeviceTileCount83271620 && !guTilingEnabled && UsingGlobalRasters())
        {
            D3DDevice_BeginTiling(gpD3DDevice, 0, gDeviceTileCount83271620, gDeviceTileRects83271628,
                                  &lrColor.m_color, lrDepthStencil.m_depth, lrDepthStencil.m_stencil);
            guTilingEnabled = 1;
        }
        else
        {
            u32 flags = lrDepthStencil.m_clearFlags;
            switch (leTarget)
            {
            case RenderTargetState::TARGETID_COLOR0: flags |= 1; break;
            case RenderTargetState::TARGETID_COLOR1: flags |= 2; break;
            case RenderTargetState::TARGETID_COLOR2: flags |= 4; break;
            case RenderTargetState::TARGETID_COLOR3: flags |= 8; break;
            case RenderTargetState::TARGETID_COLOR_ALL: flags |= 15; break;
            default: break;
            }
            D3DDevice_ClearF(gpD3DDevice, flags, 0, &lrColor.m_color, lrDepthStencil.m_depth, lrDepthStencil.m_stencil);
        }
    }

    // ARTIST82B61AA0: either of the current global surfaces may be bound.
    bool Device::UsingGlobalRasters()
    {
        D3DDevice* const lpDevice = gpD3DDevice;
        if (!gpDeviceDepthBuffer)
            return D3DResource_IsSet(gpDeviceColorBuffer, lpDevice) != 0;
        if (D3DResource_IsSet(gpDeviceColorBuffer, lpDevice))
            return true;
        return D3DResource_IsSet(gpDeviceDepthBuffer, lpDevice) != 0;
    }

    // ARTIST82B61A48: install the current native depth and colour surfaces.
    void Device::EnsureDefaultRenderTarget()
    {
        D3DSURFACES lSurfaces;
        lSurfaces.pDepthStencilSurface = gpDeviceDepthBuffer;
        lSurfaces.pRenderTarget[0] = gpDeviceColorBuffer;
        lSurfaces.pRenderTarget[1] = 0;
        lSurfaces.pRenderTarget[2] = 0;
        lSurfaces.pRenderTarget[3] = 0;
        D3DDevice_SetSurfaces(gpD3DDevice, &lSurfaces, 0);
    }

    // ARTIST82B61960: the output is the XDK's 124-byte presentation record.
    void Device::InitPresentParameters(D3DPRESENT_PARAMETERS& lrPresent, const Parameters& lrParameters)
    {
        memset(&lrPresent, 0, sizeof(lrPresent));
        lrPresent.SwapEffect = lrParameters.swapEffect;
        lrPresent.BackBufferWidth = lrParameters.backBufferWidth;
        lrPresent.BackBufferHeight = lrParameters.backBufferHeight;
        lrPresent.BackBufferCount = 1;
        lrPresent.BackBufferFormat = static_cast<D3DFORMAT>(lrParameters.backBufferFormat);
        lrPresent.AutoDepthStencilFormat = static_cast<D3DFORMAT>(lrParameters.depthStencilFormat);
        lrPresent.MultiSampleType = lrParameters.multiSampleType;
        lrPresent.FullScreen_RefreshRateInHz = lrParameters.refreshRate;
        lrPresent.Flags = lrParameters.flags;
        lrPresent.PresentationInterval = lrParameters.numVsyncs;
        lrPresent.DisableAutoBackBuffer = 1;
        lrPresent.DisableAutoFrontBuffer = 1;
        lrPresent.FrontBufferFormat = static_cast<D3DFORMAT>(lrParameters.frontBufferFormat);
        lrPresent.FrontBufferColorSpace = lrParameters.frontBufferColorSpace;
        lrPresent.RingBufferParameters.PrimarySize = lrParameters.primaryRingBufferSize;
        lrPresent.RingBufferParameters.SecondarySize = lrParameters.secondaryRingBufferSize;
        lrPresent.RingBufferParameters.SegmentCount = lrParameters.ringBufferSegmentCount;
        lrPresent.VideoScalerParameters = lrParameters.videoScalerParameters;
    }

    // ARTIST82B616B0: accepted native formats and minimum front-buffer dimensions.
    bool Device::Parameters::Validate() const
    {
        switch (static_cast<u32>(frontBufferFormat))
        {
        case 0x28280106: case 0x18280106: case 0x28280136: case 0x18280136: break;
        default: return false;
        }
        switch (static_cast<u32>(backBufferFormat))
        {
        case 0x18280186: case 0x28280186: case 0x182801B6: case 0x282801B6:
        case 0x1A2201BF: case 0x2D200199: case 0x2D22AB9F: case 0x2D20AB8D:
        case 0x1A20AB55: case 0x2DA2ABA4: case 0x1A20015A: case 0x1A22AB60:
        case 0x2D22ABA5: break;
        default: return false;
        }
        return frontBufferWidth >= 320 && frontBufferHeight >= 200;
    }

    // ARTIST82B617F0: defaults are applied only to the device-owned copy.
    void Device::Parameters::FixD3DDefaults()
    {
        if (!primaryRingBufferSize) primaryRingBufferSize = 0x8000;
        if (!secondaryRingBufferSize) secondaryRingBufferSize = 0x200000;
        if (!ringBufferSegmentCount)
            ringBufferSegmentCount = secondaryRingBufferSize > 0x1000000
                ? (secondaryRingBufferSize * 2) & 0xFFFFFC00 : 32;
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
                XVIDEO_MODE lVideoMode = {};
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

    RenderTargetState* Device::m_renderTargetState = 0;

    void Device::SetState(const RenderTargetState* lpState)
    {
        D3DDevice_SetSurfaces(gpD3DDevice, reinterpret_cast<const D3DSURFACES*>(lpState), 0);
    }

    void Device::BeginShaderStates(const ProgramVariableHandle& lrHandle,
                                  FloatShaderStateIterator& lrIterator)
    {
        D3DVECTOR4* lpConstants;
        if (lrHandle.mu8ShaderType)
            D3DDevice_BeginPixelShaderConstantF1(gpD3DDevice, lrHandle.mu8RegisterSet,
                &lpConstants, lrHandle.mu8RegisterCount);
        else
            D3DDevice_BeginVertexShaderConstantF1(gpD3DDevice, lrHandle.mu8RegisterSet,
                &lpConstants, lrHandle.mu8RegisterCount);
        lrIterator.m_dest = reinterpret_cast<rw::math::vpu::Vector4*>(lpConstants);
        lrIterator.m_handle = lrHandle;
    }

    // ARTIST 82B61F08..82B61F8C. GetTexture adds a reference; release it before
    // comparing the returned pointer and clearing each matching sampler.
    void Device::UnSetResource(D3DResource* lpResource)
    {
        D3DDevice* const lpDevice = gpD3DDevice;
        if (D3DResource_IsSet(lpResource, lpDevice))
        {
            for (DWORD luSampler = 0; luSampler < 26; ++luSampler)
            {
                D3DBaseTexture* const lpTexture = D3DDevice_GetTexture(lpDevice, luSampler);
                if (lpTexture)
                {
                    D3DResource_Release(lpTexture);
                    if (lpTexture == lpResource)
                    {
                        const UINT64 luPendingMask3 = (UINT64(1) << 63) >> (luSampler + 32);
                        D3DDevice_SetTexture(lpDevice, luSampler, 0, luPendingMask3);
                    }
                }
            }
        }
    }
}
