// XDK size optimization: ARTIST compiled the renderengine core (0x82B60000..0x82B64xxx) for
// size (record-form cr0 tests), like the state objects' resource descriptors;
// Xbox2SetTextureHeader 0x82B60B98, Texture::Destruct 0x82B62D50 (`cmpwi r3,0`) and five more
// Texture functions match only so.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "rw/rwcore_structs.h"
#include <xgraphics.h>
#include <cstring>

namespace
{
// ARTIST 82B60B98. The output addresses are the SDK's byte offsets, before
// Initialize attaches resource lane two. The helper also resets both flush words.
u32 Xbox2SetTextureHeader(renderengine::Texture* texture, u32 width, u32 height,
    u32 depth, u32 levels, renderengine::PixelFormat format,
    renderengine::Texture::Type type, u32* totalSize, u32* mipOffset)
{
    UINT baseSize;
    UINT size;
    const D3DFORMAT d3dFormat = static_cast<D3DFORMAT>(format);
    if (type == renderengine::Texture::TYPE_CUBE)
    {
        size = XGSetCubeTextureHeader(width, levels, 0, d3dFormat, D3DPOOL_DEFAULT,
            0, XGHEADER_CONTIGUOUS_MIP_OFFSET, reinterpret_cast<D3DCubeTexture*>(texture), &baseSize, 0);
    }
    else if (type == renderengine::Texture::TYPE_3D)
    {
        size = XGSetVolumeTextureHeader(width, height, depth, levels, 0, d3dFormat,
            D3DPOOL_DEFAULT, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET,
            reinterpret_cast<D3DVolumeTexture*>(texture), &baseSize, 0);
    }
    else if (type == renderengine::Texture::TYPE_ARRAY)
    {
        size = XGSetArrayTextureHeader(width, height, depth, levels, 0, d3dFormat,
            D3DPOOL_DEFAULT, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0,
            reinterpret_cast<D3DArrayTexture*>(texture), &baseSize, 0);
    }
    else if (type == renderengine::Texture::TYPE_1D)
    {
        size = XGSetLineTextureHeader(width, levels, 0, d3dFormat, D3DPOOL_DEFAULT,
            0, XGHEADER_CONTIGUOUS_MIP_OFFSET, reinterpret_cast<D3DLineTexture*>(texture), &baseSize, 0);
    }
    else
    {
        size = XGSetTextureHeader(width, height, levels, 0, d3dFormat, D3DPOOL_DEFAULT,
            0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, reinterpret_cast<D3DTexture*>(texture), &baseSize, 0);
    }
    if (texture)
    {
        texture->BaseFlush = D3DFLUSH_INITIAL_VALUE;
        texture->MipFlush = D3DFLUSH_INITIAL_VALUE;
        texture->Format.MipAddress = 0;
    }
    if (totalSize) *totalSize = size;
    if (mipOffset)
    {
        if (size != baseSize)
            *mipOffset = baseSize;
        else
            *mipOffset = 0;
    }
    return size;
}
}

namespace renderengine
{
// ARTIST 82B629D8. Multiple mip levels use the greatest power of two not
// exceeding the supplied width/height, as in the original integer loops.
Texture* Texture::Initialize(const rw::Resource& resourceRef, const Parameters& parametersRef)
{
    const rw::Resource* resource = &resourceRef;
    const Parameters* parameters = &parametersRef;
    Texture* texture = static_cast<Texture*>(resource->m_baseResources[0]);
    std::memset(texture, 0, sizeof(Texture));
    u32 width = parameters->muWidth;
    u32 height = parameters->muHeight;
    u32 levels = parameters->muNumLevels;
    if (!levels)
    {
        u32 w = width, h = height;
        levels = 1;
        while (w > 1 || h > 1)
        {
            if (w >= 2) w >>= 1;
            if (h >= 2) h >>= 1;
            ++levels;
        }
    }
    if (levels > 1)
    {
        u32 bits = 0;
        for (u32 w = width >> 1; w; w >>= 1) ++bits;
        width = 1u << bits;
        bits = 0;
        for (u32 h = height >> 1; h; h >>= 1) ++bits;
        height = 1u << bits;
    }
    // ARTIST passes both the total size and the mip offset to the header builder.
    u32 totalSize;
    u32 mipOffset;
    Xbox2SetTextureHeader(texture, width, height, parameters->muDepth, levels,
        parameters->meFormat, parameters->meType, &totalSize, &mipOffset);
    if (!(parameters->muFlags & FLAG_DONTALLOCATE))
    {
        const u32 address = reinterpret_cast<u32>(resource->m_baseResources[2]);
        texture->Format.BaseAddress = address >> GPU_TEXTURE_ADDRESS_SHIFT;
        texture->Format.MipAddress = levels > 1 ? (address + mipOffset) >> GPU_TEXTURE_ADDRESS_SHIFT : 0;
        texture->Xbox2CheckPhysicalMemoryFlags();
    }
    return texture;
}

void Texture::Xbox2CheckPhysicalMemoryFlags()
{
    Texture* texture = this;
    const u32 address = texture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;
    if (address)
    {
        const DWORD flags = XQueryMemoryProtect(reinterpret_cast<void*>(address));
        if (flags & (PAGE_NOCACHE | PAGE_WRITECOMBINE))
            texture->Common &= ~D3DCOMMON_CPU_CACHED_MEMORY;
        else
            texture->Common |= D3DCOMMON_CPU_CACHED_MEMORY;
    }
}

Texture::Type Texture::GetType() const
{
    Type type = TYPE_NA;
    const u32 dimension = Format.Dimension;
    if (dimension == GPUDIMENSION_1D)
        type = TYPE_1D;
    else if (dimension == GPUDIMENSION_2D)
        type = Format.Stacked ? TYPE_ARRAY : TYPE_2D;
    else if (dimension == GPUDIMENSION_CUBEMAP)
        type = TYPE_CUBE;
    else if (dimension == GPUDIMENSION_3D)
        type = TYPE_3D;
    return type;
}

u32 Texture::GetWidth() const
{
    const Texture* texture = this;
    switch (texture->GetType())
    {
    case TYPE_1D: return texture->Format.Size.OneD.Width + 1;
    case TYPE_2D: case TYPE_ARRAY: case TYPE_CUBE: return texture->Format.Size.TwoD.Width + 1;
    case TYPE_3D: return texture->Format.Size.ThreeD.Width + 1;
    default: return 0;
    }
}

u32 Texture::GetHeight() const
{
    const Texture* texture = this;
    switch (texture->GetType())
    {
    case TYPE_1D: return 1;
    case TYPE_2D: case TYPE_ARRAY: case TYPE_CUBE: return texture->Format.Size.TwoD.Height + 1;
    case TYPE_3D: return texture->Format.Size.ThreeD.Height + 1;
    default: return 0;
    }
}

u32 Texture::GetDepth() const
{
    const Texture* texture = this;
    switch (texture->GetType())
    {
    case TYPE_1D: case TYPE_2D: case TYPE_ARRAY: case TYPE_CUBE: return 1;
    case TYPE_3D: return texture->Format.Size.ThreeD.Depth + 1;
    default: return 0;
    }
}

PixelFormat Texture::GetFormat() const
{
    const Texture* texture = this;
    const GPUTEXTURE_FETCH_CONSTANT& f = texture->Format;
    return static_cast<PixelFormat>(MAKED3DFMT2(f.DataFormat, f.Endian, f.Tiled,
        f.SignX, f.SignY, f.SignZ, f.SignW, f.NumFormat,
        f.SwizzleX, f.SwizzleY, f.SwizzleZ, f.SwizzleW));
}

void Texture::GetParameters(Parameters& parameters) const
{
    XGTEXTURE_DESC desc;
    XGGetTextureDesc(const_cast<Texture*>(this), 0, &desc);
    if (desc.ResourceType == D3DRTYPE_TEXTURE)
    {
        parameters.muDepth = 1;
        parameters.meType = TYPE_2D;
    }
    else if (desc.ResourceType == D3DRTYPE_VOLUMETEXTURE)
    {
        parameters.muDepth = desc.Depth;
        parameters.meType = TYPE_3D;
    }
    else if (desc.ResourceType == D3DRTYPE_CUBETEXTURE)
    {
        parameters.muDepth = 1;
        parameters.meType = TYPE_CUBE;
    }
    else if (desc.ResourceType == D3DRTYPE_ARRAYTEXTURE)
    {
        parameters.muDepth = desc.Depth;
        parameters.meType = TYPE_ARRAY;
    }
    else if (desc.ResourceType == D3DRTYPE_LINETEXTURE)
    {
        parameters.meType = TYPE_1D;
        parameters.muDepth = 1;
    }
    else
    {
        parameters.meType = TYPE_NA;
    }
    parameters.muFlags = 0;
    parameters.muWidth = desc.Width;
    parameters.muHeight = desc.Height;
    parameters.meFormat = static_cast<PixelFormat>(desc.Format);
    parameters.muNumLevels = Format.MaxMipLevel + 1;
}

u32 Texture::GetPixelDataSize(Type type, u32 width, u32 height, u32 depth, PixelFormat format, u32 levels)
{
    u32 size = 0;
    Xbox2SetTextureHeader(0, width, height, depth, levels, format, type, &size, 0);
    return size;
}

bool Texture::Lock(u32 flags, u32 level, u32 index, Locked& lockedRef)
{
    Texture* texture = this;
    Locked* locked = &lockedRef;
    Device::UnSetResource(texture);
    DWORD lockFlags = 0;
    if (!(flags & LOCK_WRITE) && XMemGetPageSize(reinterpret_cast<void*>(texture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT)) <= 4096)
        lockFlags = D3DLOCK_READONLY;
    const Type type = texture->GetType();
    if (type == TYPE_3D)
    {
        D3DLOCKED_BOX box;
        D3DVolumeTexture_LockBox(reinterpret_cast<D3DVolumeTexture*>(texture), level, &box, 0, lockFlags);
        locked->muSliceStride = box.SlicePitch;
        locked->mpPixelData = box.pBits;
        locked->muStride = box.RowPitch;
    }
    else if (type >= TYPE_1D && type <= TYPE_CUBE)
    {
        D3DLOCKED_RECT rect;
        switch (type)
        {
        case TYPE_1D: D3DLineTexture_LockRect(reinterpret_cast<D3DLineTexture*>(texture), level, &rect, 0, lockFlags); break;
        case TYPE_2D: D3DTexture_LockRect(reinterpret_cast<D3DTexture*>(texture), level, &rect, 0, lockFlags); break;
        case TYPE_ARRAY: D3DArrayTexture_LockRect(reinterpret_cast<D3DArrayTexture*>(texture), index, level, &rect, 0, lockFlags); break;
        case TYPE_CUBE: D3DCubeTexture_LockRect(reinterpret_cast<D3DCubeTexture*>(texture), static_cast<D3DCUBEMAP_FACES>(index), level, &rect, 0, lockFlags); break;
        }
        locked->muSliceStride = 0;
        locked->mpPixelData = rect.pBits;
        locked->muStride = rect.Pitch;
    }
    locked->mpTexture = texture;
    locked->muLockFlags = flags;
    locked->muMipLevel = static_cast<u8>(level);
    locked->muIndex = static_cast<u8>(index);
    locked->muWidth = static_cast<u16>(texture->GetWidth() >> level);
    if (!locked->muWidth) locked->muWidth = 1;
    locked->muHeight = static_cast<u16>(texture->GetHeight() >> level);
    if (!locked->muHeight) locked->muHeight = 1;
    locked->muVolumeDepth = static_cast<u16>(texture->GetDepth() >> level);
    if (!locked->muVolumeDepth) locked->muVolumeDepth = 1;
    return true;
}

void Texture::Unlock(const Locked& lockedRef)
{
    Texture* texture = this;
    const Locked* locked = &lockedRef;
    switch (texture->GetType())
    {
    case TYPE_1D: D3DLineTexture_UnlockRect(reinterpret_cast<D3DLineTexture*>(texture), locked->muMipLevel); break;
    case TYPE_2D: D3DTexture_UnlockRect(reinterpret_cast<D3DTexture*>(texture), locked->muMipLevel); break;
    case TYPE_ARRAY: D3DArrayTexture_UnlockRect(reinterpret_cast<D3DArrayTexture*>(texture), locked->muIndex, locked->muMipLevel); break;
    case TYPE_CUBE: D3DCubeTexture_UnlockRect(reinterpret_cast<D3DCubeTexture*>(texture), static_cast<D3DCUBEMAP_FACES>(locked->muIndex), locked->muMipLevel); break;
    case TYPE_3D: D3DVolumeTexture_UnlockBox(reinterpret_cast<D3DVolumeTexture*>(texture), locked->muMipLevel); break;
    }
}

void Texture::Destruct(Texture* texture)
{
    D3DDevice* device = gpD3DDevice;
    Device::UnSetResource(texture);
    if (!D3DResource_IsSet(texture, device))
    {
        if (texture->Fence)
        {
            D3DResource_BlockUntilNotBusy(texture);
            texture->Fence = 0;
        }
        Parameters parameters = { TYPE_NA, 0, 0, 0, 0, 0, static_cast<PixelFormat>(0) };
        texture->GetParameters(parameters);
        u32 size = 0;
        Xbox2SetTextureHeader(0, parameters.muWidth, parameters.muHeight,
            parameters.muDepth, parameters.muNumLevels, parameters.meFormat, parameters.meType, &size, 0);
        D3DDevice_InvalidateGpuCache(device, reinterpret_cast<void*>(texture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT), size, 0);
    }
}

void Texture::Release()
{
    Texture* texture = this;
    if (texture->Common)
    {
        Destruct(texture);
        if (texture->Common & D3DCOMMON_D3DCREATED)
            D3DResource_Release(texture);
        // ARTIST clears the base address and preserves the low fetch-format bits.
        const u32 format = texture->Format.dword[1] & (D3DTEXTURE_ALIGNMENT - 1);
        texture->Common = 0;
        texture->Format.dword[1] = format;
    }
}
}
