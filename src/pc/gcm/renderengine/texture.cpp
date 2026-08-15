#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/device.h"   // gDevice
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // WriteToLog (create-failure diagnostics)
#include "rw/rwcore_structs.h"   // rw::Resource (the rw-resource Initialize overload's memory block)

#include <d3d9.h>
#include <cstring>   // memcpy
#include <cstdio>    // snprintf
#include <cstddef>   // offsetof
#include <new>       // placement new (build the texture object into rw resource memory)

// PC / D3D9 implementation of the renderengine 2D-texture create + upload path. The
// X360/PS3 renderengine marshals a platform resource descriptor and a GPU surface
// format (the loading screen passes format 340); on PC these map onto a managed D3D9
// texture in 32-bit A8R8G8B8 (BGRA byte order). Lock/Unlock expose the top mip for the
// caller to copy pixels into.

namespace renderengine
{
    // Pin the Locked descriptor's recovered byte layout (pointer-invariant facts; uncalled). The
    // X360 CgsNetworkImageConverter unpack path reads pixelData (+0x04 on X360, where the mpTexture
    // pointer is 4 bytes), stride (+0x08) and height (+0x0E) off this block; on the x64 PC target the
    // mpTexture/mpPixelData pointers widen so only the post-pointer field ORDER is invariant. These
    // assert the fields the Lock(Locked*) overload fills sit where the descriptor's readers expect.
    static void _AssertLockedLayout()
    {
        static_assert(offsetof(Texture::Locked, mpTexture) == 0, "Locked.mpTexture is first");
        static_assert(offsetof(Texture::Locked, muStride) > offsetof(Texture::Locked, mpPixelData),
                      "stride follows the pixel-data pointer");
        static_assert(offsetof(Texture::Locked, muHeight) > offsetof(Texture::Locked, muWidth),
                      "height follows width");
        static_assert(offsetof(Texture::Locked, muWidth) == offsetof(Texture::Locked, muStride) + 4,
                      "geometry block follows the stride word");
    }

    // Compute the storage a texture of these parameters needs (W*H*4 for the 32-bit
    // format), recorded the same way as the other renderengine resource descriptors.
    Texture2D::ResourceDescriptor* Texture2D::GetResourceDescriptor(ResourceDescriptor* lpDescriptorOut,
                                                                   const Parameters* lpParams)
    {
        const u32 luBytes = lpParams->muWidth * lpParams->muHeight * 4u;
        lpDescriptorOut->mauData[0] = luBytes;
        lpDescriptorOut->mauData[1] = 16u;   // alignment
        return lpDescriptorOut;
    }

    // Create the D3D9 texture and wrap it in a renderengine Texture2D.
    Texture2D* Texture2D::Initialize(const ResourceDescriptor* /*lpDescriptor*/, const Parameters* lpParams)
    {
        if (gDevice == nullptr)
        {
            return nullptr;
        }

        const UINT luLevels = (lpParams->muNumLevels != 0u) ? lpParams->muNumLevels : 1u;
        IDirect3DTexture9* lpD3DTexture = nullptr;
        if (FAILED(gDevice->CreateTexture(lpParams->muWidth, lpParams->muHeight, luLevels, 0,
                                          D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &lpD3DTexture, nullptr)))
        {
            return nullptr;
        }

        Texture2D* lpTexture = new Texture2D();
        lpTexture->mpD3DTexture = lpD3DTexture;
        return lpTexture;
    }

    void Texture::Lock(Texture* lpTexture, s32 liLevel, s32 /*liFace*/, s32 liFlags, LockInfo* lpLockInfoOut)
    {
        lpLockInfoOut->mpBits = nullptr;
        lpLockInfoOut->muPitch = 0u;
        if (lpTexture == nullptr || lpTexture->mpD3DTexture == nullptr)
        {
            return;
        }

        IDirect3DTexture9* lpD3DTexture = static_cast<IDirect3DTexture9*>(lpTexture->mpD3DTexture);
        D3DLOCKED_RECT lLockedRect;
        if (SUCCEEDED(lpD3DTexture->LockRect(static_cast<UINT>(liLevel), &lLockedRect, nullptr,
                                             static_cast<DWORD>(liFlags))))
        {
            lpLockInfoOut->mpBits = lLockedRect.pBits;
            lpLockInfoOut->muPitch = static_cast<u32>(lLockedRect.Pitch);
        }
    }

    void Texture::Unlock(Texture* lpTexture, LockInfo* /*lpLockInfo*/)
    {
        if (lpTexture == nullptr || lpTexture->mpD3DTexture == nullptr)
        {
            return;
        }
        static_cast<IDirect3DTexture9*>(lpTexture->mpD3DTexture)->UnlockRect(0u);
    }

    // The Locked* form of the same X360 entry point (see texture.h). The descriptor is unread on
    // this backend, exactly as in the LockInfo overload above.
    void Texture::Unlock(Texture* lpTexture, Locked* /*lpLocked*/)
    {
        Unlock(lpTexture, static_cast<LockInfo*>(nullptr));
    }

    namespace
    {
        // Bytes per 4x4 block for the block-compressed (DXT/BCn) D3DFORMATs; 0 otherwise.
        u32 lFormatBytesPerBlock(s32 liFormat)
        {
            switch (liFormat)
            {
            case 827611204: return 8u;    // D3DFMT_DXT1 / BC1
            case 844388420:               // D3DFMT_DXT2
            case 861165636:               // D3DFMT_DXT3 / BC2
            case 877942852:               // D3DFMT_DXT4
            case 894720068: return 16u;   // D3DFMT_DXT5 / BC3
            default:        return 0u;
            }
        }

        // Bits per pixel for the uncompressed D3DFORMATs Burnout rasters use (D3DFORMAT enum
        // values, see rwgraphics_structs.h). Unknown formats fall back to 32 bpp.
        u32 lFormatBitsPerPixel(s32 liFormat)
        {
            switch (liFormat)
            {
            case 20: return 24u;                                                  // R8G8B8
            case 21: case 22: case 31: case 32: case 33: case 34: case 35: case 67:
                return 32u;                                                       // *8R8G8B8 / A2*10 / G16R16
            case 23: case 24: case 25: case 26: case 27: case 30: case 40:
            case 51: case 60: case 61: case 64:
                return 16u;                                                       // 16-bit colour / A8L8 / bump
            case 28: case 41: case 50: case 52:
                return 8u;                                                        // A8 / P8 / L8 / A4L4
            case 36: return 64u;                                                  // A16B16G16R16
            default: return 32u;
            }
        }

        // Byte size of one mip level (block math for DXT, pitch*height for linear).
        u32 lMipBytes(s32 liFormat, u32 luWidth, u32 luHeight)
        {
            const u32 luBytesPerBlock = lFormatBytesPerBlock(liFormat);
            if (luBytesPerBlock != 0u)
            {
                u32 luBlocksW = (luWidth + 3u) / 4u;
                u32 luBlocksH = (luHeight + 3u) / 4u;
                if (luBlocksW == 0u) luBlocksW = 1u;
                if (luBlocksH == 0u) luBlocksH = 1u;
                return luBlocksW * luBlocksH * luBytesPerBlock;
            }
            return ((luWidth * lFormatBitsPerPixel(liFormat) + 7u) / 8u) * luHeight;
        }
    }

    u32 Texture::GetPixelDataSize(s32 liFormat, u32 luWidth, u32 luHeight, u32 luDepth, u32 luNumLevels)
    {
        if (luNumLevels == 0u) luNumLevels = 1u;
        u32 luTotal = 0u;
        u32 luW = luWidth, luH = luHeight, luD = (luDepth != 0u ? luDepth : 1u);
        for (u32 luLevel = 0u; luLevel < luNumLevels; ++luLevel)
        {
            luTotal += lMipBytes(liFormat, luW, luH) * luD;
            luW = (luW > 1u) ? (luW >> 1) : 1u;
            luH = (luH > 1u) ? (luH >> 1) : 1u;
            luD = (luD > 1u) ? (luD >> 1) : 1u;
        }
        return luTotal;
    }

    // Read the raster header into Parameters. Faithful to X360 0x82B60D80, with the X360 GPU
    // path (XGGetTextureDesc + Xenos-format -> internal-format switch) replaced by reading the
    // PC raster's stored D3DFORMAT/size directly (PC stores them; the console reads them back
    // from the GPU descriptor).
    void Texture::GetParameters(const Texture* lpTexture, Parameters* lpParamsOut)
    {
        if (lpTexture == nullptr)
        {
            lpParamsOut->miFormat = -1;
            lpParamsOut->muSysMem = 0u;
            lpParamsOut->muWidth = 0u;
            lpParamsOut->muHeight = 0u;
            lpParamsOut->muDepth = 0u;
            lpParamsOut->muNumLevels = 0u;
            lpParamsOut->muReserved0 = 0u;
            lpParamsOut->muReserved1 = 0u;
            return;
        }
        lpParamsOut->miFormat = lpTexture->miFormat;
        lpParamsOut->muSysMem = 0u;
        lpParamsOut->muWidth = lpTexture->muWidth;
        lpParamsOut->muHeight = lpTexture->muHeight;
        lpParamsOut->muDepth = lpTexture->muDepth;
        lpParamsOut->muNumLevels = lpTexture->muNumMipLevels;
        lpParamsOut->muReserved0 = 0u;
        lpParamsOut->muReserved1 = 0u;
    }

    // Write the rw::BaseResourceDescriptors<5> (five {u32 size, u32 align}) for a texture.
    // Faithful to X360 0x823FF848: slot0 = the Texture object; slots 1/3/4 unused; slot2 = the
    // pixel data (page aligned) unless the texture lives in system memory. The X360 hardcoded
    // slot0 = {52, 4} for its 52-byte object; the PC object size is sizeof(Texture) (x64).
    void Texture::GetResourceDescriptor(u32* lpDescriptorOut, const Parameters* lpParams)
    {
        lpDescriptorOut[0] = static_cast<u32>(sizeof(Texture));  // slot0 size (X360: 0x34)
        lpDescriptorOut[1] = 8u;                                 // slot0 align (X360: 4; x64: 8)
        lpDescriptorOut[2] = 0u;  lpDescriptorOut[3] = 1u;       // slot1
        lpDescriptorOut[4] = 0u;  lpDescriptorOut[5] = 1u;       // slot2 (set below)
        lpDescriptorOut[6] = 0u;  lpDescriptorOut[7] = 1u;       // slot3
        lpDescriptorOut[8] = 0u;  lpDescriptorOut[9] = 1u;       // slot4
        if ((lpParams->muSysMem & 1u) == 0u)
        {
            lpDescriptorOut[4] = GetPixelDataSize(lpParams->miFormat, lpParams->muWidth,
                                                  lpParams->muHeight, lpParams->muDepth,
                                                  lpParams->muNumLevels);
            lpDescriptorOut[5] = 4096u;  // graphics-local page alignment (X360)
        }
    }

    // Create the managed D3D9 texture from the parameters and upload the mip chain. The PC body
    // of RwRasterResourceType::FixUp.
    void Texture::Create(Texture* lpTexture, const Parameters* lpParams, const void* lpPixelData)
    {
        lpTexture->miFormat = lpParams->miFormat;
        lpTexture->muWidth = static_cast<u16>(lpParams->muWidth);
        lpTexture->muHeight = static_cast<u16>(lpParams->muHeight);
        lpTexture->muDepth = static_cast<u8>(lpParams->muDepth);
        lpTexture->muNumMipLevels = static_cast<u8>(lpParams->muNumLevels);
        lpTexture->mpD3DTexture = nullptr;

        if (gDevice == nullptr || lpParams->miFormat < 0)
        {
            return;
        }

        const UINT luLevels = (lpParams->muNumLevels != 0u) ? lpParams->muNumLevels : 1u;
        IDirect3DTexture9* lpD3DTexture = nullptr;
        const HRESULT lhrCreate = gDevice->CreateTexture(lpParams->muWidth, lpParams->muHeight, luLevels, 0,
                                                         static_cast<D3DFORMAT>(lpParams->miFormat),
                                                         D3DPOOL_MANAGED, &lpD3DTexture, nullptr);
        if (FAILED(lhrCreate))
        {
            char lac[160];
            std::snprintf(lac, sizeof(lac),
                "[Texture] CreateTexture(fmt=%d %ux%u mips=%u MANAGED) failed hr=0x%08X\n",
                lpParams->miFormat, lpParams->muWidth, lpParams->muHeight,
                static_cast<unsigned>(luLevels), static_cast<unsigned>(lhrCreate));
            CgsDev::Log::WriteToLog(lac);
            return;
        }
        lpTexture->mpD3DTexture = lpD3DTexture;

        if (lpPixelData == nullptr)
        {
            return;  // created empty (the data pipeline supplies pixels later)
        }

        // Upload each mip: the loader stores the chain TIGHTLY PACKED in graphics-local memory
        // (the serialised pixel block the bundle carries; tools/assets/bundles/x360_tex.py
        // produces exactly that from the console's tiled + packed-mip-tail layout). Copy row by
        // row against the locked pitch rather than whole-mip: a managed surface is free to hand
        // back a padded pitch, which for the small mips of a compressed chain does not equal the
        // tight one.
        const u8* lpSource = static_cast<const u8*>(lpPixelData);
        const u32 luBytesPerBlock = lFormatBytesPerBlock(lpParams->miFormat);
        u32 luW = lpParams->muWidth, luH = lpParams->muHeight;
        for (UINT luLevel = 0u; luLevel < luLevels; ++luLevel)
        {
            const u32 luMipBytes = GetPixelDataSize(lpParams->miFormat, luW, luH, 1u, 1u);
            // Rows of the SOURCE image: block rows for a compressed format, pixel rows otherwise.
            u32 luRowBytes, luNumRows;
            if (luBytesPerBlock != 0u)
            {
                luRowBytes = ((luW + 3u) / 4u) * luBytesPerBlock;
                luNumRows = (luH + 3u) / 4u;
            }
            else
            {
                luNumRows = luH;
                luRowBytes = (luNumRows != 0u) ? (luMipBytes / luNumRows) : luMipBytes;
            }
            if (luRowBytes == 0u) luRowBytes = luMipBytes;
            if (luNumRows == 0u) luNumRows = 1u;

            D3DLOCKED_RECT lLockedRect;
            if (SUCCEEDED(lpD3DTexture->LockRect(luLevel, &lLockedRect, nullptr, 0)))
            {
                u8* lpDest = static_cast<u8*>(lLockedRect.pBits);
                const u32 luDestPitch = static_cast<u32>(lLockedRect.Pitch);
                if (luDestPitch == luRowBytes)
                {
                    memcpy(lpDest, lpSource, luMipBytes);
                }
                else
                {
                    for (u32 luRow = 0u; luRow < luNumRows; ++luRow)
                    {
                        memcpy(lpDest + luRow * luDestPitch, lpSource + luRow * luRowBytes,
                               luRowBytes);
                    }
                }
                lpD3DTexture->UnlockRect(luLevel);
            }
            lpSource += luMipBytes;
            luW = (luW > 1u) ? (luW >> 1) : 1u;
            luH = (luH > 1u) ? (luH >> 1) : 1u;
        }
    }

    // Release the realised D3D texture. The PC body of RwRasterResourceType::FixDown.
    void Texture::Destroy(Texture* lpTexture)
    {
        if (lpTexture == nullptr || lpTexture->mpD3DTexture == nullptr)
        {
            return;
        }
        lpTexture->mpD3DTexture->Release();
        lpTexture->mpD3DTexture = nullptr;
    }

    // X360 GetType @0x82B60E68 decodes header+0x30 bits 21-22 (line/2D/array/cube/volume) and, for
    // the 2D case, header+0x20 bit 21 (the array flag). The PC raster has no GPU header: every
    // renderengine texture the create path produces is a managed D3D 2D texture, so the type is 2D.
    Texture::Type Texture::GetType(const Texture* /*lpTexture*/)
    {
        return E_TYPE_2D;
    }

    // X360 GetWidth @0x82B60EC8 / GetHeight @0x82B60F38: the X360 spine reads the packed GPU header
    // (and returns 1 for a line texture); the PC raster stores width/height explicitly. Both X360
    // bodies bias the stored field by +1 because the header packs (dimension-1); the PC raster
    // already stores the true dimension (Create copies muWidth/muHeight straight through), so no bias.
    u32 Texture::GetWidth(const Texture* lpTexture)
    {
        if (lpTexture == nullptr)
        {
            return 1u;
        }
        return lpTexture->muWidth;
    }

    u32 Texture::GetHeight(const Texture* lpTexture)
    {
        if (lpTexture == nullptr)
        {
            return 1u;
        }
        return lpTexture->muHeight;
    }

    // X360 GetDepth @0x82B60FA8: returns 1 for everything except a volume texture, where it reads the
    // packed depth. The PC raster is always 2D (depth 1); honour the stored depth if one was set.
    u32 Texture::GetDepth(const Texture* lpTexture)
    {
        if (lpTexture == nullptr || lpTexture->muDepth == 0u)
        {
            return 1u;
        }
        return lpTexture->muDepth;
    }

    // X360 GetFormat @0x82B61000: re-packs the GPU fetch-constant swizzle/format bitfields back into
    // a D3DFORMAT. The PC raster keeps the D3DFORMAT explicitly, so return it directly.
    s32 Texture::GetFormat(const Texture* lpTexture)
    {
        if (lpTexture == nullptr)
        {
            return -1;
        }
        return lpTexture->miFormat;
    }

    // X360 Lock @0x82B62B20 (Locked* overload): lock the requested mip/face surface and fill the full
    // Locked descriptor -- the texture, the surface bits/strides, and the per-mip geometry. The X360
    // spine dispatches on GetType to the matching D3D*Texture_LockRect; the PC raster is always a 2D
    // texture, so it locks the 2D rect. Geometry is (dimension >> mip), floored to 1 (matching the
    // X360's `if (!(Width >> level)) = 1`). muVolumeDepth / muSliceStride stay 0 (no volume on PC).
    void Texture::Lock(Texture* lpTexture, s32 liLevel, s32 liFace, s32 liFlags, Locked* lpLockedOut)
    {
        lpLockedOut->mpTexture     = lpTexture;
        lpLockedOut->mpPixelData   = nullptr;
        lpLockedOut->muStride      = 0u;
        lpLockedOut->muSliceStride = 0u;
        lpLockedOut->mu8MipLevel   = static_cast<u8>(liLevel);
        lpLockedOut->mu8Index      = static_cast<u8>(liFace);
        lpLockedOut->muLockFlags   = static_cast<u32>(liFlags);

        if (lpTexture != nullptr && lpTexture->mpD3DTexture != nullptr)
        {
            IDirect3DTexture9* lpD3DTexture = static_cast<IDirect3DTexture9*>(lpTexture->mpD3DTexture);
            D3DLOCKED_RECT lLockedRect;
            if (SUCCEEDED(lpD3DTexture->LockRect(static_cast<UINT>(liLevel), &lLockedRect, nullptr,
                                                 static_cast<DWORD>(liFlags))))
            {
                lpLockedOut->mpPixelData = lLockedRect.pBits;
                lpLockedOut->muStride    = static_cast<u32>(lLockedRect.Pitch);
            }
        }

        u32 luWidth = GetWidth(lpTexture) >> liLevel;
        if (luWidth == 0u) luWidth = 1u;
        lpLockedOut->muWidth = static_cast<u16>(luWidth);

        u32 luHeight = GetHeight(lpTexture) >> liLevel;
        if (luHeight == 0u) luHeight = 1u;
        lpLockedOut->muHeight = static_cast<u16>(luHeight);

        u32 luDepth = GetDepth(lpTexture) >> liLevel;
        if (luDepth == 0u) luDepth = 1u;
        lpLockedOut->muVolumeDepth = static_cast<u16>(luDepth);
    }

    // X360 Xbox2CheckPhysicalMemoryFlags @0x82B60D28: queries the physical-memory protection of the
    // texture's pixel page (XQueryMemoryProtect) and toggles the header's write-combined flag. The PC
    // managed-pool raster does not expose a physical page; no flag to set, so this is a no-op.
    u32 Texture::Xbox2CheckPhysicalMemoryFlags(Texture* /*lpTexture*/)
    {
        return 0u;
    }

    // X360 Destruct @0x82B62D50: unbind the texture from every sampler, wait for it to go idle, and
    // free its GPU memory. The PC backend's equivalent of freeing the GPU resource is releasing the
    // D3D texture (Destroy); the unbind/idle wait is handled by D3D9's own reference counting.
    void Texture::Destruct(Texture* lpTexture)
    {
        Destroy(lpTexture);
    }

    // X360 Release @0x82B62E18: if the texture is live, Destruct it (and, when it owns its D3D
    // resource, release that), then clear the live word so the object can be reused. On PC the live
    // state is simply whether mpD3DTexture is set; Destruct/Destroy clears it.
    Texture* Texture::Release(Texture* lpTexture)
    {
        if (lpTexture != nullptr && lpTexture->mpD3DTexture != nullptr)
        {
            Destruct(lpTexture);
        }
        return lpTexture;
    }

    // X360 Texture::Initialize @0x82B629D8 (rw-resource overload): build the texture object into the
    // resource memory the allocator handed out (slot0 = the object, slot2 = the page-aligned pixel
    // data) and lay its header out from the serialised Parameters. The X360 spine writes the GPU
    // texture header in place via Xbox2SetTextureHeader; the PC raster instead creates a managed D3D
    // texture and (if pixel data is present) uploads it -- the same split GetResourceDescriptor /
    // Create / Texture2D::Initialize already use. Returns the texture object (slot0).
    Texture* Texture::Initialize(rw::Resource* lpResourceMemory, const Parameters* lpParams)
    {
        // Slot0 of the resource block is the Texture object's storage; slot2 is the pixel data
        // (null when the texture is system-memory-only / created empty).
        void* lpObjectMemory = lpResourceMemory->m_baseResources[0];
        const void* lpPixelData = lpResourceMemory->m_baseResources[2];

        Texture* lpTexture = (lpObjectMemory != nullptr)
                                 ? new (lpObjectMemory) Texture()
                                 : new Texture();
        Create(lpTexture, lpParams, lpPixelData);
        return lpTexture;
    }
}
