#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/device.h"   // gDevice

#include <d3d9.h>

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
}
