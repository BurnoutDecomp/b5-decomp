#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "rw/rwcore_structs.h"
#include <xgraphics.h>
#include <cstring>

namespace renderengine
{
namespace
{
// ARTIST 832716C0/832716C4, both initially zero in the original image.
u32 nextEDRAM = 0;
u32 nextHierarchicalZ = 0;
}

// ARTIST inlined descriptor at Target::CreateColor 824034D0 / CreateDepth82403688.
rw::ResourceDescriptor PixelBuffer::GetResourceDescriptor(const Parameters&)
{
    rw::ResourceDescriptor descriptor;
    for (u32 i = 0; i != 5; ++i)
    {
        descriptor.m_baseResourceDescriptors[i].m_size = 0;
        descriptor.m_baseResourceDescriptors[i].m_alignment = 1;
    }
    descriptor.m_baseResourceDescriptors[0].m_size = sizeof(PixelBuffer);
    descriptor.m_baseResourceDescriptors[0].m_alignment = 4;
    return descriptor;
}

// ARTIST 82B621A8. The cursors count EDRAM/Hierarchical-Z tiles, not bytes.
PixelBuffer* PixelBuffer::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    PixelBuffer* const buffer = static_cast<PixelBuffer*>(resource.m_baseResources[0]);
    std::memset(buffer, 0, sizeof(PixelBuffer));
    D3DSURFACE_PARAMETERS sdkParameters;
    sdkParameters.Base = 0;
    sdkParameters.HierarchicalZBase = 0;
    UINT hierarchicalZSize;
    if (!(parameters.flags & FLAG_DONTALLOCATE))
    {
        const UINT size = XGSurfaceSize(parameters.width, parameters.height,
            static_cast<D3DFORMAT>(parameters.format), static_cast<D3DMULTISAMPLE_TYPE>(parameters.multiSampleType));
        u32 base = nextEDRAM;
        if (base + size > 0x800) base = 0;
        sdkParameters.Base = base;
        nextEDRAM = base + size;
        if (parameters.type == TYPE_DEPTHSTENCIL)
        {
            u32 width = parameters.width;
            u32 height = parameters.height;
            if (static_cast<s32>(parameters.multiSampleType) >= 1) height *= 2;
            if (parameters.multiSampleType == 2) width *= 2;
            const u32 tiles = (((width + 31) >> 5) * ((height + 15) >> 4)) & 0x7FFFFF;
            hierarchicalZSize = tiles;
            if (tiles > 0xE10)
                sdkParameters.HierarchicalZBase = ~0u;
            else
            {
                u32 zBase = nextHierarchicalZ;
                if (zBase + tiles > 0xE10) zBase = 0;
                sdkParameters.HierarchicalZBase = zBase;
                nextHierarchicalZ = zBase + tiles;
            }
        }
    }
    sdkParameters.ColorExpBias = parameters.colorExpBias;
    XGSetSurfaceHeader(parameters.width, parameters.height, static_cast<D3DFORMAT>(parameters.format),
        static_cast<D3DMULTISAMPLE_TYPE>(parameters.multiSampleType), &sdkParameters, buffer, &hierarchicalZSize);
    buffer->Fence = 0;
    buffer->BaseFlush = D3DFLUSH_INITIAL_VALUE;
    if (!(parameters.flags & FLAG_DONTALLOCATE)) buffer->Common |= D3DCOMMON_SURFACE_D3D_EDRAM_ALLOCATED;
    buffer->m_type = parameters.type;
    return buffer;
}

// ARTIST 82B60870. Both native surface kinds encode the EDRAM base at +1C.
void PixelBuffer::Xbox2SetBaseEDRAM(s16 base)
{
    if (m_type == TYPE_RENDERTARGET || m_type == TYPE_DEPTHSTENCIL)
        ColorInfo.ColorBase = static_cast<u32>(base);
}

// ARTIST 82B60898.
void PixelBuffer::Xbox2SetBaseHierarchicalZ(u32 base)
{
    HiControl.HiBaseAddr = base;
}

// ARTIST 82B62300. The float argument consumes its native parameter slot, so
// clearStencil arrives at incoming SP+5C; clearColor is in r10, clearZ in f1.
bool PixelBuffer::Xbox2ResolveTo(Texture* texture, u32 flags, u32 x, u32 y,
    u32 destLevel, u32 destSlice, const D3DVECTOR4* clearColor, float clearZ, u32 clearStencil)
{
    D3DDevice* const device = ::gpD3DDevice;
    PixelBuffer* original;
    if (m_type == TYPE_DEPTHSTENCIL)
    {
        D3DDevice_SetDepthStencilSurface(device, this);
        flags |= 4;
        original = &gOriginalDepthStencilBuffer;
    }
    else
    {
        D3DDevice_SetRenderTarget(device, 0, this);
        original = &gOriginalColorBuffer;
    }
    if (this == original && guTilingEnabled)
        D3DDevice_EndTiling(device, flags, 0, texture, clearColor, clearZ, clearStencil, 0);
    else
    {
        D3DRECT rectangle;
        rectangle.x1 = x;
        rectangle.y1 = y;
        u32 width = Width + 1;
        if (texture->GetWidth() < width) width = texture->GetWidth();
        rectangle.x2 = x + width;
        u32 height = Height + 1;
        if (texture->GetHeight() < height) height = texture->GetHeight();
        rectangle.y2 = y + height;
        D3DDevice_Resolve(device, flags, &rectangle, texture, 0, destLevel,
            destSlice, clearColor, clearZ, clearStencil, 0);
    }
    return true;
}
}
