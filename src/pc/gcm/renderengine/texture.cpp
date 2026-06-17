#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/device.h"   // gDevice

#include <d3d9.h>
#include <cstring>   // memcpy

// PC / D3D9 implementation of the renderengine 2D-texture create + upload path. The
// X360/PS3 renderengine marshals a platform resource descriptor and a GPU surface
// format (the loading screen passes format 340); on PC these map onto a managed D3D9
// texture in 32-bit A8R8G8B8 (BGRA byte order). Lock/Unlock expose the top mip for the
// caller to copy pixels into.

namespace renderengine
{
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
        if (FAILED(gDevice->CreateTexture(lpParams->muWidth, lpParams->muHeight, luLevels, 0,
                                          static_cast<D3DFORMAT>(lpParams->miFormat),
                                          D3DPOOL_MANAGED, &lpD3DTexture, nullptr)))
        {
            return;
        }
        lpTexture->mpD3DTexture = lpD3DTexture;

        if (lpPixelData == nullptr)
        {
            return;  // created empty (the data pipeline supplies pixels later)
        }

        // Upload each mip: the loader stores the chain tightly packed in graphics-local memory.
        // Copy whole mips (managed power-of-two textures lock at the tight pitch); a row-by-row
        // path can refine this once the bundle pixel layout is finalised by the data pipeline.
        const u8* lpSource = static_cast<const u8*>(lpPixelData);
        u32 luW = lpParams->muWidth, luH = lpParams->muHeight;
        for (UINT luLevel = 0u; luLevel < luLevels; ++luLevel)
        {
            const u32 luMipBytes = GetPixelDataSize(lpParams->miFormat, luW, luH, 1u, 1u);
            D3DLOCKED_RECT lLockedRect;
            if (SUCCEEDED(lpD3DTexture->LockRect(luLevel, &lLockedRect, nullptr, 0)))
            {
                memcpy(lLockedRect.pBits, lpSource, luMipBytes);
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
}
