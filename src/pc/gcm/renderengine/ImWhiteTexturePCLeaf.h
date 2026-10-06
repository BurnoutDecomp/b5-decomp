#pragma once
#include <d3d9.h>
#include <mutex>
#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/TextureUploadPCLeaf.h"

namespace renderengine {
// FLAG PC-platform leaf: native storage for ARTIST ConstructWhiteTexture
// 827F1AE8. The original creates one4x4 level and writes16 opaque-white pixels.
// Share this home between ordinary immediate draws and the GUI mask consumer.
inline Texture* GetImmediateWhiteTexturePC(IDirect3DDevice9* lpDevice)
{
    struct WhiteTexturePC
    {
        std::mutex mMutex;
        IDirect3DDevice9* mpDevice = nullptr;
        Texture mTexture = {};
        ~WhiteTexturePC() { if (mTexture.mpD3DTexture) mTexture.mpD3DTexture->Release(); }
    };
    static WhiteTexturePC white;
    std::lock_guard<std::mutex> lLock(white.mMutex);
    if (!lpDevice) return nullptr;
    if (white.mpDevice != lpDevice)
    {
        if (white.mTexture.mpD3DTexture) white.mTexture.mpD3DTexture->Release();
        white.mTexture = Texture{};
        white.mpDevice = lpDevice;
    }
    if (!white.mTexture.mpD3DTexture)
    {
        IDirect3DTexture9* lpWhite = nullptr;
        if (FAILED(TextureUploadPC::Create2D(lpDevice, 4, 4, 1, D3DFMT_A8R8G8B8, &lpWhite)))
            return nullptr;
        D3DLOCKED_BOX lLocked = {};
        if (FAILED(TextureUploadPC::Lock(lpWhite, 0, 0, 0, lLocked)))
        { lpWhite->Release(); return nullptr; }
        for (u32 luY = 0; luY < 4; ++luY)
        {
            auto* lpRow = reinterpret_cast<u32*>(static_cast<u8*>(lLocked.pBits) + luY*lLocked.RowPitch);
            for (u32 luX = 0; luX < 4; ++luX) lpRow[luX] = 0xFFFFFFFFu;
        }
        if (FAILED(TextureUploadPC::Unlock(lpWhite, 0, 0)))
        { lpWhite->Release(); return nullptr; }
        white.mTexture.miFormat = D3DFMT_A8R8G8B8;
        white.mTexture.muWidth = white.mTexture.muHeight = 4;
        white.mTexture.muDepth = white.mTexture.muNumMipLevels = 1;
        white.mTexture.mpD3DTexture = lpWhite;
    }
    return &white.mTexture;
}
}
