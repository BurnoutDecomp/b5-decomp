#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture2d.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rendertargetstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "rw/rwcore_structs.h"
#include <xgraphics.h>

D3DDevice* gpD3DDevice = 0;

// ARTIST82B61F90..82B61FD8. Keep the provisional original identity until its
// declaration name is recovered; this is distinct from Device::UnSetResource.
void sub_82B61F90(const renderengine::VertexDescriptorData* lpDescriptor)
{
    D3DVertexDeclaration* const lpDeclaration = lpDescriptor->mpDeclaration;
    D3DDevice* const lpDevice = ::gpD3DDevice;
    if (D3DVertexDeclaration_IsSet(lpDeclaration, lpDevice))
    {
        D3DDevice_SetVertexDeclaration(lpDevice, 0);
    }
}

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
    u32 gDeviceTileCount83271620 = 0;
    D3DRECT gDeviceTileRects83271628[8];
    u32 gDeviceWord83271604 = 0;
    struct FrontBufferSlot { Texture2D texture; u8 padding[12]; };
    CGS_STATIC_ASSERT((sizeof(FrontBufferSlot) == 64), "ARTIST front buffer stride");
    __declspec(align(16)) FrontBufferSlot gFrontBuffers83271580[2];
    Texture2D* gpFrontBuffer83271618 = 0;
    u32 gFrontBufferIndex832716B8 = 0;
    u32 gFrontBufferCount832716BC = 0;

    // ARTIST82B61850: clear device-owned native state before default parameters.
    bool Device::Initialize()
    {
        gDeviceWord83271604 = 1;
        gpD3DDevice = 0;
        gpDeviceColorBuffer = 0;
        gpDeviceDepthBuffer = 0;
        gpFrontBuffer83271618 = 0;
        guTilingEnabled = 0;
        gDeviceTileCount83271620 = 0;
        memset(gDeviceTileRects83271628, 0, sizeof(gDeviceTileRects83271628));
        gDeviceFrameCounter83271624 = 0;
        gDeviceCounter832716A8 = 0;
        gDeviceCounter832716AC = 0;
        gDevicePerformanceCounter832716B0.QuadPart = 0;
        gFrontBufferIndex832716B8 = 0;
        gFrontBufferCount832716BC = 0;
        memset(&gOriginalColorBuffer, 0, sizeof(gOriginalColorBuffer));
        memset(&gOriginalDepthStencilBuffer, 0, sizeof(gOriginalDepthStencilBuffer));
        memset(gFrontBuffers83271580, 0, sizeof(gFrontBuffers83271580));
        m_parameterCopy.Initialize(6);
        m_currentState = STATE_INITIALIZE;
        return true;
    }

    // ARTIST82B63298: resolve, rotate, fence, synchronize and present in order.
    void Device::ShowPixelBuffer(PixelBuffer* lpBuffer)
    {
        D3DDevice* const lpDevice = gpD3DDevice;
        gpFrontBuffer83271618 = &gFrontBuffers83271580[gFrontBufferIndex832716B8].texture;
        if (gFrontBufferCount832716BC == 1)
            D3DDevice_SynchronizeToPresentationInterval(lpDevice);
        lpBuffer->Xbox2ResolveTo(gpFrontBuffer83271618, 0, 0, 0, 0, 0, 0, 1.0f, 0);
        guTilingEnabled = 0;
        gFrontBufferIndex832716B8 = (gFrontBufferIndex832716B8 + 1) % gFrontBufferCount832716BC;
        gDeviceCounter832716AC = gDeviceCounter832716A8;
        gDeviceCounter832716A8 = D3DDevice_InsertFence(lpDevice);
        if (gFrontBufferCount832716BC != 1)
            D3DDevice_SynchronizeToPresentationInterval(lpDevice);
        D3DDevice_Swap(lpDevice, gpFrontBuffer83271618, &m_parameterCopy.videoScalerParameters);
    }

    // ARTIST82B63398: block for the displayed texture and expose its lock.
    bool Device::LockFrontBuffer(LockedFrontBuffer& lrLockedFrontBuffer)
    {
        D3DDevice_BlockUntilIdle(gpD3DDevice);
        Texture::Locked lLocked;
        if (gpFrontBuffer83271618->Lock(Texture::LOCK_READ, 0, 0, lLocked))
        {
            lrLockedFrontBuffer.pixelData = lLocked.mpPixelData;
            lrLockedFrontBuffer.format = gpFrontBuffer83271618->GetFormat();
            lrLockedFrontBuffer.pitch = lLocked.muStride;
            lrLockedFrontBuffer.width = lLocked.muWidth;
            lrLockedFrontBuffer.height = lLocked.muHeight;
            return true;
        }
        lrLockedFrontBuffer.pixelData = 0;
        lrLockedFrontBuffer.format = static_cast<PixelFormat>(-1);
        lrLockedFrontBuffer.pitch = 0;
        lrLockedFrontBuffer.width = 0;
        lrLockedFrontBuffer.height = 0;
        return false;
    }

    // ARTIST82B63450: reconstruct the texture lock fields used by Unlock.
    void Device::UnlockFrontBuffer(LockedFrontBuffer& lrLockedFrontBuffer)
    {
        Texture::Locked lLocked;
        lLocked.mpTexture = gpFrontBuffer83271618;
        lLocked.mpPixelData = const_cast<void*>(lrLockedFrontBuffer.pixelData);
        lLocked.muStride = lrLockedFrontBuffer.pitch;
        lLocked.muWidth = lrLockedFrontBuffer.width;
        lLocked.muHeight = lrLockedFrontBuffer.height;
        lLocked.muMipLevel = 0;
        lLocked.muIndex = 0;
        gpFrontBuffer83271618->Unlock(lLocked);
    }

    // ARTIST82B63840: allocate Xenos EDRAM surfaces and native front textures.
    void Device::InitializeGlobalBuffers()
    {
        const u32 width = m_parameterCopy.backBufferWidth;
        const u32 height = m_parameterCopy.backBufferHeight;
        u32 tileHeight = height;
        const D3DMULTISAMPLE_TYPE msaa = m_parameterCopy.multiSampleType;
        const PixelFormat colorFormat = m_parameterCopy.backBufferFormat;
        const PixelFormat depthFormat = m_parameterCopy.depthStencilFormat;
        u32 size = XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(colorFormat), msaa);
        if (m_parameterCopy.depthStencilAutoCreate)
            size += XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(depthFormat), msaa);
        if (size > 0x800)
        {
            u32 count = 1;
            do
            {
                ++count;
                u32 h = height / count;
                h = (msaa == 1 || msaa == 2) ? (h + 7) & ~7u : (h + 15) & ~15u;
                tileHeight = (h + 31) & ~31u;
                size = XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(colorFormat), msaa);
                if (m_parameterCopy.depthStencilAutoCreate)
                    size += XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(depthFormat), msaa);
            } while (size > 0x800);
            gDeviceTileCount83271620 = count;
            gDeviceTileRects83271628[0].x1 = 0;
            gDeviceTileRects83271628[0].y1 = 0;
            gDeviceTileRects83271628[0].x2 = width;
            gDeviceTileRects83271628[0].y2 = tileHeight;
            for (u32 i = 1; i < count; ++i)
            {
                gDeviceTileRects83271628[i].x1 = 0;
                gDeviceTileRects83271628[i].y1 = gDeviceTileRects83271628[i-1].y2;
                gDeviceTileRects83271628[i].x2 = width;
                gDeviceTileRects83271628[i].y2 = gDeviceTileRects83271628[i].y1 + tileHeight;
            }
            if (gDeviceTileRects83271628[count-1].y2 > height)
                gDeviceTileRects83271628[count-1].y2 = height;
        }
        else gDeviceTileCount83271620 = 0;

        PixelBuffer::Parameters surface = { PixelBuffer::TYPE_RENDERTARGET, 0, width, tileHeight, colorFormat, msaa, 0 };
        rw::Resource resource;
        resource.m_baseResources[0] = &gOriginalColorBuffer;
        gpDeviceColorBuffer = PixelBuffer::Initialize(resource, surface);
        if (m_parameterCopy.depthStencilAutoCreate)
        {
            surface.type = PixelBuffer::TYPE_DEPTHSTENCIL;
            surface.format = depthFormat;
            resource = rw::Resource();
            resource.m_baseResources[0] = &gOriginalDepthStencilBuffer;
            gpDeviceDepthBuffer = PixelBuffer::Initialize(resource, surface);
        }
        else gpDeviceDepthBuffer = 0;
        Texture2D::Parameters front = { 0, m_parameterCopy.frontBufferWidth, m_parameterCopy.frontBufferHeight, 1, m_parameterCopy.frontBufferFormat };
        rw::ResourceDescriptor descriptor = Texture2D::GetResourceDescriptor(front);
        descriptor.m_baseResourceDescriptors[0].m_size = 0;
        descriptor.m_baseResourceDescriptors[0].m_alignment = 1;
        resource = rw::ResourceAllocatorRegistry::GetDefaultAllocator()->DoAllocate(descriptor, 0);
        resource.m_baseResources[0] = &gFrontBuffers83271580[0].texture;
        gpFrontBuffer83271618 = Texture2D::Initialize(resource, front);
        if (gDeviceTileCount83271620)
        {
            resource = rw::ResourceAllocatorRegistry::GetDefaultAllocator()->DoAllocate(descriptor, 0);
            resource.m_baseResources[0] = &gFrontBuffers83271580[1].texture;
            Texture2D::Initialize(resource, front);
        }
        guTilingEnabled = 0;
        gFrontBufferIndex832716B8 = 0;
        gFrontBufferCount832716BC = gDeviceTileCount83271620 ? 2 : 1;
        EnsureDefaultRenderTarget();
    }

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


    // ARTIST82B61D78: depth/stencil-only clear or the first tile pass.
    void Device::Clear(const ClearDepthStencilParameters& lrDepthStencil)
    {
        if (gDeviceTileCount83271620 && !guTilingEnabled && UsingGlobalRasters())
        {
            D3DDevice_BeginTiling(gpD3DDevice, 0, gDeviceTileCount83271620, gDeviceTileRects83271628,
                                  0, lrDepthStencil.m_depth, lrDepthStencil.m_stencil);
            guTilingEnabled = 1;
        }
        else
        {
            D3DDevice_ClearF(gpD3DDevice, lrDepthStencil.m_clearFlags, 0, 0,
                             lrDepthStencil.m_depth, lrDepthStencil.m_stencil);
        }
    }

    // ARTIST82B61C88: colour-only clear uses depth 1 and stencil 0 directly.
    void Device::Clear(const ClearColorParameters& lrColor, RenderTargetState::TargetID leTarget)
    {
        if (gDeviceTileCount83271620 && !guTilingEnabled && UsingGlobalRasters())
        {
            D3DDevice_BeginTiling(gpD3DDevice, 0, gDeviceTileCount83271620, gDeviceTileRects83271628,
                                  &lrColor.m_color, 1.0f, 0);
            guTilingEnabled = 1;
        }
        else
        {
            u32 flags = 0;
            switch (leTarget)
            {
            case RenderTargetState::TARGETID_COLOR0: flags = 1; break;
            case RenderTargetState::TARGETID_COLOR1: flags = 2; break;
            case RenderTargetState::TARGETID_COLOR2: flags = 4; break;
            case RenderTargetState::TARGETID_COLOR3: flags = 8; break;
            case RenderTargetState::TARGETID_COLOR_ALL: flags = 15; break;
            default: break;
            }
            D3DDevice_ClearF(gpD3DDevice, flags, 0, &lrColor.m_color, 1.0f, 0);
        }
    }

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
