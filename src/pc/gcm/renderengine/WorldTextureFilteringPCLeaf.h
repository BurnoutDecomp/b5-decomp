#pragma once

#include "pc/gcm/renderengine/GraphicsSettingsPCLeaf.h"
#include "pc/gcm/renderengine/SamplerStateCachePCLeaf.h"

namespace renderengine
{
    // FLAG PC-platform leaf: optional native texture filtering. ARTIST's world
    // samplers remain trilinear at setting 1; higher settings improve shallow
    // texture footprints without changing authored mip data or LOD bias.
    inline void ApplyWorldTextureFilteringPC(IDirect3DDevice9* lpDevice, u32 luUnit, bool lbCubeRaster)
    {
        u32 luAnisotropy = 1u;
        const u32 luRequested = static_cast<u32>(GetGraphicsSettingsPC().miAnisotropicFiltering);
        if (!lbCubeRaster && luRequested > 1u)
        {
            static IDirect3DDevice9* spCapsDevice = nullptr;
            static u32 suMaxAnisotropy = 1u;
            if (spCapsDevice != lpDevice)
            {
                D3DCAPS9 lCaps = {};
                suMaxAnisotropy = 1u;
                if (SUCCEEDED(lpDevice->GetDeviceCaps(&lCaps)) &&
                    (lCaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0u)
                    suMaxAnisotropy = lCaps.MaxAnisotropy > 1u ? lCaps.MaxAnisotropy : 1u;
                spCapsDevice = lpDevice;
            }
            luAnisotropy = luRequested < suMaxAnisotropy ? luRequested : suMaxAnisotropy;
        }
        PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MINFILTER,
            luAnisotropy > 1u ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
        PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        PCSetSamplerState(lpDevice, luUnit, D3DSAMP_MAXANISOTROPY, luAnisotropy);
    }
}
