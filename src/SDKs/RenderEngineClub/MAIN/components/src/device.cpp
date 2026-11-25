#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture2d.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rendertargetstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "rw/rwcore_structs.h"
#include <xgraphics.h>
#include <cstring>

// ARTIST 82B618B4 and 82B63BE8 call the XDK CRT providers.
#pragma function(memcpy, memset)

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
    struct PresentationFences
    {
        u32 current;
        u32 previous;
    };
    PresentationFences gPresentationFences; // ARTIST 832716A8 and 832716AC.
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
        gPresentationFences.current = 0;
        gPresentationFences.previous = 0;
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
        gPresentationFences.previous = gPresentationFences.current;
        gPresentationFences.current = D3DDevice_InsertFence(lpDevice);
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

    // ARTIST82B63840: fit the color and depth surfaces in EDRAM, then
    // allocate physical texture storage for the one or two native front buffers.
    void Device::InitializeGlobalBuffers()
    {
        const u32 width = m_parameterCopy.backBufferWidth;
        const u32 height = m_parameterCopy.backBufferHeight;
        const D3DMULTISAMPLE_TYPE multiSampleType = m_parameterCopy.multiSampleType;
        const bool createDepthStencil = m_parameterCopy.depthStencilAutoCreate;
        const PixelFormat colorFormat = m_parameterCopy.backBufferFormat;
        u32 tileHeight = height;
        u32 size = XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(colorFormat), multiSampleType);
        const PixelFormat depthFormat = m_parameterCopy.depthStencilFormat;
        if (createDepthStencil)
            size += XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(depthFormat), multiSampleType);

        if (size <= GPU_EDRAM_TILES)
        {
            gDeviceTileCount83271620 = 0;
        }
        else
        {
            u32 tileCount = 1;
            do
            {
                ++tileCount;
                const u32 heightPerTile = height / tileCount;
                const u32 alignedHeight =
                    (multiSampleType == D3DMULTISAMPLE_2_SAMPLES ||
                     multiSampleType == D3DMULTISAMPLE_4_SAMPLES)
                    ? (heightPerTile + GPU_EDRAM_TILE_HEIGHT_2X - 1) & ~(GPU_EDRAM_TILE_HEIGHT_2X - 1)
                    : (heightPerTile + GPU_EDRAM_TILE_HEIGHT_1X - 1) & ~(GPU_EDRAM_TILE_HEIGHT_1X - 1);
                tileHeight = (alignedHeight + 31) & ~31u;
                size = XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(colorFormat), multiSampleType);
                if (createDepthStencil)
                    size += XGSurfaceSize(width, tileHeight, static_cast<D3DFORMAT>(depthFormat), multiSampleType);
            }
            while (size > GPU_EDRAM_TILES);

            gDeviceTileCount83271620 = tileCount;
            D3DRECT* tile = gDeviceTileRects83271628;
            tile->x1 = 0;
            tile->y1 = 0;
            tile->x2 = width;
            tile->y2 = tileHeight;
            for (u32 remaining = tileCount - 1; remaining != 0; --remaining)
            {
                const LONG top = tile->y2;
                ++tile;
                tile->x1 = 0;
                tile->x2 = width;
                tile->y1 = top;
                tile->y2 = top + tileHeight;
            }
            if (static_cast<u32>(tile->y2) > height)
                tile->y2 = height;
        }

        {
            const PixelBuffer::Parameters color = {
                PixelBuffer::TYPE_RENDERTARGET, 0, width, tileHeight, colorFormat, multiSampleType, 0
            };
            void* colorMemory = &gOriginalColorBuffer;
            const rw::Resource colorResource(colorMemory);
            gpDeviceColorBuffer = PixelBuffer::Initialize(colorResource, color);
        }

        if (createDepthStencil)
        {
            const PixelBuffer::Parameters depth = {
                PixelBuffer::TYPE_DEPTHSTENCIL, 0, width, tileHeight, m_parameterCopy.depthStencilFormat,
                m_parameterCopy.multiSampleType, 0
            };
            void* depthMemory = &gOriginalDepthStencilBuffer;
            const rw::Resource depthResource(depthMemory);
            gpDeviceDepthBuffer = PixelBuffer::Initialize(depthResource, depth);
        }
        else
        {
            gpDeviceDepthBuffer = 0;
        }

        const Texture2D::Parameters front = {
            0, m_parameterCopy.frontBufferWidth, m_parameterCopy.frontBufferHeight, 1,
            m_parameterCopy.frontBufferFormat
        };
        rw::ResourceDescriptor descriptor = Texture2D::GetResourceDescriptor(front);
        descriptor.m_baseResourceDescriptors[0] = rw::BaseResourceDescriptor();
        rw::Resource resource = rw::ResourceAllocatorRegistry::GetDefaultAllocator()->Allocate(descriptor, 0);
        resource.m_baseResources[0] = &gFrontBuffers83271580[0].texture;
        gpFrontBuffer83271618 = Texture2D::Initialize(resource, front);
        if (gDeviceTileCount83271620)
        {
            resource = rw::ResourceAllocatorRegistry::GetDefaultAllocator()->Allocate(descriptor, 0);
            resource.m_baseResources[0] = &gFrontBuffers83271580[1].texture;
            Texture2D::Initialize(resource, front);
        }
        guTilingEnabled = 0;
        gFrontBufferIndex832716B8 = 0;
        gFrontBufferCount832716BC = 1 + (gDeviceTileCount83271620 != 0);
        EnsureDefaultRenderTarget();
    }

    // ARTIST82B63BA8. InitializeGlobalBuffers owns the real surface/texture setup.
    bool Device::Start(const Parameters& lrParameters)
    {
        if (!lrParameters.Validate()) return false;
        memcpy(&m_parameterCopy, &lrParameters, sizeof(m_parameterCopy));
        m_parameterCopy.FixD3DDefaults();
        D3DPRESENT_PARAMETERS lPresent;
        InitPresentParameters(lPresent, lrParameters);
        if (Direct3D_CreateDevice(0, D3DDEVTYPE_HAL, 0, lrParameters.behaviorFlags, &lPresent, &gpD3DDevice) != 0)
            return false;
        gPresentationFences.current = 0;
        gPresentationFences.previous = 0;
        InitializeGlobalBuffers();
        D3DDevice_SetVertexDeclaration(gpD3DDevice, 0);
        RenderTargetState::Parameters lTargetParameters = { { gpDeviceColorBuffer, 0, 0, 0, gpDeviceDepthBuffer } };
        rw::ResourceDescriptor lDescriptor(sizeof(RenderTargetState), 4);
        rw::Resource lResource = rw::ResourceAllocatorRegistry::GetDefaultAllocator()->Allocate(lDescriptor, 0);
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
            D3DDevice_BeginPixelShaderConstantF1(gpD3DDevice, lrHandle.mu8RegisterIndex,
                &lpConstants, lrHandle.mu8RegisterCount);
        else
            D3DDevice_BeginVertexShaderConstantF1(gpD3DDevice, lrHandle.mu8RegisterIndex,
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

    // ARTIST 82B62088 unbinds the active shader before ProgramBuffer::Release poisons its type.
    void Device::UnSetResource(const ProgramBufferData* lpProgram)
    {
        D3DDevice* const lpDevice = gpD3DDevice;
        D3DResource* const lpShader = reinterpret_cast<D3DResource*>(const_cast<ProgramBufferData*>(lpProgram + 1));
        if (lpProgram->miShaderType == 0)
        {
            if (D3DResource_IsSet(lpShader, lpDevice))
                D3DDevice_SetVertexShader(lpDevice, 0);
        }
        else
        {
            if (D3DResource_IsSet(lpShader, lpDevice))
                D3DDevice_SetPixelShader(lpDevice, 0);
        }
    }
}
