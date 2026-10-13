#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/SamplerStateCache.h"

#include <cstring>

namespace renderengine
{
    // ARTIST sub_827E8950 passes these Xenos min/mag values to the XDK setters.
    // Only values attested by the recovered callers are translated here; unknown
    // Xenos values leave the current D3D9 state alone instead of guessing.
    inline bool XenonMinMagFilterToD3D9(u8 luFilter, DWORD* lpFilterOut)
    {
        switch (luFilter)
        {
        case 0: *lpFilterOut = D3DTEXF_POINT;       return true;
        case 1: *lpFilterOut = D3DTEXF_LINEAR;      return true;
        case 4: *lpFilterOut = D3DTEXF_ANISOTROPIC; return true;
        default: return false;
        }
    }

    // The Xenos MipFilter setter writes the two-bit value directly. Its pinned
    // NONE/POINT/LINEAR values match the corresponding D3D9 MIPFILTER values.
    inline bool XenonMipFilterToD3D9(u8 luFilter, DWORD* lpFilterOut)
    {
        switch (luFilter)
        {
        case 0: *lpFilterOut = D3DTEXF_NONE;   return true;
        case 1: *lpFilterOut = D3DTEXF_POINT;  return true;
        case 2: *lpFilterOut = D3DTEXF_LINEAR; return true;
        default: return false;
        }
    }

    // ARTIST caller data pins Xenos 0=WRAP and 2=CLAMP. DecFIGS' PS3 address
    // enumeration differs from the X360 encoding, so the remaining Xenos values
    // are intentionally left unmapped until an X360 producer attests them.
    inline bool XenonAddressToD3D9(u8 luAddress, DWORD* lpAddressOut)
    {
        switch (luAddress)
        {
        case 0: *lpAddressOut = D3DTADDRESS_WRAP;  return true;
        case 2: *lpAddressOut = D3DTADDRESS_CLAMP; return true;
        default: return false;
        }
    }

    inline bool QuerySamplerCapsPC(IDirect3DDevice9* lpDevice, D3DCAPS9* lpCapsOut)
    {
        static IDirect3DDevice9* spCapsDevice = nullptr;
        static D3DCAPS9 sCaps = {};
        static bool sbCapsValid = false;

        if (spCapsDevice != lpDevice)
        {
            spCapsDevice = lpDevice;
            std::memset(&sCaps, 0, sizeof(sCaps));
            sbCapsValid = lpDevice != nullptr && SUCCEEDED(lpDevice->GetDeviceCaps(&sCaps));
        }

        if (sbCapsValid && lpCapsOut != nullptr)
            *lpCapsOut = sCaps;
        return sbCapsValid;
    }

    inline DWORD XenonFilterCapsForTextureTypePC(D3DRESOURCETYPE leTextureType,
                                                  bool lbHaveCaps,
                                                  const D3DCAPS9& lCaps)
    {
        if (!lbHaveCaps)
            return 0u;
        if (leTextureType == D3DRTYPE_VOLUMETEXTURE)
            return lCaps.VolumeTextureFilterCaps;
        if (leTextureType == D3DRTYPE_CUBETEXTURE)
            return lCaps.CubeTextureFilterCaps;
        return lCaps.TextureFilterCaps;
    }

    inline void ApplyXenonMinMagFiltersPC(IDirect3DDevice9* lpDevice, u32 luUnit,
                                           const u8* lpBlock, bool lbHaveCaps,
                                           DWORD luFilterCaps)
    {
        DWORD luMinFilter = 0;
        if (XenonMinMagFilterToD3D9(lpBlock[0x12], &luMinFilter))
        {
            if (luMinFilter == D3DTEXF_ANISOTROPIC &&
                (!lbHaveCaps || (luFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) == 0u))
                luMinFilter = D3DTEXF_LINEAR;
            PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MINFILTER, luMinFilter);
        }

        DWORD luMagFilter = 0;
        if (XenonMinMagFilterToD3D9(lpBlock[0x14], &luMagFilter))
        {
            if (luMagFilter == D3DTEXF_ANISOTROPIC &&
                (!lbHaveCaps || (luFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) == 0u))
                luMagFilter = D3DTEXF_LINEAR;
            PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MAGFILTER, luMagFilter);
        }
    }

    inline void ApplyXenonMaxAnisotropyPC(IDirect3DDevice9* lpDevice, u32 luUnit,
                                          const u8* lpBlock, bool lbHaveCaps,
                                          DWORD luFilterCaps, const D3DCAPS9& lCaps)
    {
        // The engine field is a requested anisotropy count; D3D9 uses a count
        // too, but the XDK table that encodes the Xenos register is not copied.
        // Clamp the native request to the bound texture type's real device cap.
        DWORD luMaxAnisotropy = 1u;
        if (lbHaveCaps &&
            (luFilterCaps & (D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC)) != 0u &&
            lCaps.MaxAnisotropy > 1u)
        {
            luMaxAnisotropy = lpBlock[0x17];
            if (luMaxAnisotropy < 1u)
                luMaxAnisotropy = 1u;
            if (luMaxAnisotropy > lCaps.MaxAnisotropy)
                luMaxAnisotropy = lCaps.MaxAnisotropy;
        }
        PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MAXANISOTROPY, luMaxAnisotropy);
    }

    inline void ApplyXenonTextureDependentFilterFieldsPC(
        IDirect3DDevice9* lpDevice, u32 luUnit, const u8* lpBlock,
        D3DRESOURCETYPE leTextureType, bool lbHaveCaps, const D3DCAPS9& lCaps,
        bool lbApplyMinMagFilters, bool lbApplyMaxAnisotropy)
    {
        const DWORD luFilterCaps =
            XenonFilterCapsForTextureTypePC(leTextureType, lbHaveCaps, lCaps);
        if (lbApplyMinMagFilters)
            ApplyXenonMinMagFiltersPC(lpDevice, luUnit, lpBlock, lbHaveCaps, luFilterCaps);
        if (lbApplyMaxAnisotropy)
            ApplyXenonMaxAnisotropyPC(lpDevice, luUnit, lpBlock, lbHaveCaps,
                                      luFilterCaps, lCaps);
    }

    inline void ReapplyXenonTextureDependentFilterFieldsPC(
        IDirect3DDevice9* lpDevice, u32 luUnit, const void* lpSamplerBlock)
    {
        if (lpDevice == nullptr || lpSamplerBlock == nullptr || luUnit >= 16u)
            return;

        IDirect3DBaseTexture9* lpTexture = nullptr;
        const HRESULT lhrGetTexture = lpDevice->GetTexture(luUnit, &lpTexture);
        if (FAILED(lhrGetTexture) || lpTexture == nullptr)
        {
            if (lpTexture != nullptr)
                lpTexture->Release();
            return;
        }

        D3DCAPS9 lCaps = {};
        const bool lbHaveCaps = QuerySamplerCapsPC(lpDevice, &lCaps);
        ApplyXenonTextureDependentFilterFieldsPC(
            lpDevice, luUnit, static_cast<const u8*>(lpSamplerBlock),
            lpTexture->GetType(), lbHaveCaps, lCaps, true, true);
        lpTexture->Release();
    }

    // D3DDevice_SetTexture reapplies the independent cached Xenos MaxMipLevel
    // request to the incoming texture. Type-dependent filter fields are handled
    // separately from the raw request cache.
    inline void ApplyXenonMaxMipRequestPC(IDirect3DDevice9* lpDevice, u32 luUnit,
                                         u32 luRequest)
    {
        if (lpDevice == nullptr || luUnit >= 16u)
            return;

        IDirect3DBaseTexture9* lpTexture = nullptr;
        const HRESULT lhrGetTexture = lpDevice->GetTexture(luUnit, &lpTexture);
        if (FAILED(lhrGetTexture) || lpTexture == nullptr)
        {
            if (lpTexture != nullptr)
                lpTexture->Release();
            return;
        }

        const DWORD luLevelCount = lpTexture->GetLevelCount();
        if (luLevelCount != 0u)
        {
            DWORD luMaxMipLevel = luRequest & 0x0Fu;
            const DWORD luLastLevel = luLevelCount - 1u;
            if (luMaxMipLevel > luLastLevel)
                luMaxMipLevel = luLastLevel;
            PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MAXMIPLEVEL, luMaxMipLevel);
        }
        lpTexture->Release();
    }

    // Realize fields with a D3D9 counterpart from the original 32-byte Xenos
    // sampler block. The original MaxMipLevel setter writes max(texture-header
    // bits 26..29, request) to Xenos bits 26..29, while caching the raw request.
    // D3D9 exposes a zero-based MAXMIPLEVEL index, so this PC leaf clamps the
    // request to the bound chain. The bind hook replays the independent cached
    // raw request after the filter-only type-dependent realization.
    // The Xenos header-derived bound is not separately available.
    // The separate Xenos MinMipLevel field, Z-filter controls, AnisotropyBias,
    // border-enable bit, and remaining packed register bits have no pinned
    // D3D9 equivalent here and remain unapplied.
    inline void ApplyXenonSamplerBlockPC(IDirect3DDevice9* lpDevice, u32 luUnit,
                                         const void* lpSamplerBlock)
    {
        if (lpDevice == nullptr || lpSamplerBlock == nullptr || luUnit >= 16u)
            return;

        const u8* const lpBlock = static_cast<const u8*>(lpSamplerBlock);

        IDirect3DBaseTexture9* lpTexture = nullptr;
        const HRESULT lhrGetTexture = lpDevice->GetTexture(luUnit, &lpTexture);
        if (FAILED(lhrGetTexture))
        {
            if (lpTexture != nullptr)
                lpTexture->Release();
            lpTexture = nullptr;
        }
        const D3DRESOURCETYPE leTextureType =
            lpTexture != nullptr ? lpTexture->GetType() : D3DRTYPE_TEXTURE;

        D3DCAPS9 lCaps = {};
        const bool lbHaveCaps = QuerySamplerCapsPC(lpDevice, &lCaps);

        DWORD luAddress = 0;
        if (XenonAddressToD3D9(lpBlock[0x08], &luAddress))
            PCSetSamplerState(lpDevice, luUnit, D3DSAMP_ADDRESSU, luAddress);
        if (XenonAddressToD3D9(lpBlock[0x09], &luAddress))
            PCSetSamplerState(lpDevice, luUnit, D3DSAMP_ADDRESSV, luAddress);
        if (XenonAddressToD3D9(lpBlock[0x0A], &luAddress))
            PCSetSamplerState(lpDevice, luUnit, D3DSAMP_ADDRESSW, luAddress);

        ApplyXenonTextureDependentFilterFieldsPC(
            lpDevice, luUnit, lpBlock, leTextureType, lbHaveCaps, lCaps,
            true, false);

        DWORD luMipFilter = 0;
        if (XenonMipFilterToD3D9(lpBlock[0x0C], &luMipFilter))
            PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MIPFILTER, luMipFilter);

        ApplyXenonTextureDependentFilterFieldsPC(
            lpDevice, luUnit, lpBlock, leTextureType, lbHaveCaps, lCaps,
            false, true);

        // MipMapLodBias is a float in Xenos state +0x00 and D3D9 accepts its
        // float bit pattern through the DWORD-valued SetSamplerState interface.
        DWORD luLodBiasBits = 0;
        std::memcpy(&luLodBiasBits, lpBlock + 0x00, sizeof(luLodBiasBits));
        PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MIPMAPLODBIAS, luLodBiasBits);

        // ARTIST 0x8293CD68 writes max(texture-header bits 26..29, request) into
        // Xenos bits 26..29 and caches the raw request for the later bind. D3D9
        // has no Xenos header field, so clamp the authored request to the actual
        // chain here; default request 0 is valid for every non-empty chain. The
        // separate MinMipLevel field (default request 13) remains unsupported.
        if (lpTexture != nullptr)
        {
            const DWORD luLevelCount = lpTexture->GetLevelCount();
            if (luLevelCount != 0u)
            {
                DWORD luMaxMipLevel = lpBlock[0x18] & 0x0Fu;
                const DWORD luLastLevel = luLevelCount - 1u;
                if (luMaxMipLevel > luLastLevel)
                    luMaxMipLevel = luLastLevel;
                PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MAXMIPLEVEL, luMaxMipLevel);
            }
            lpTexture->Release();
        }
    }
}
