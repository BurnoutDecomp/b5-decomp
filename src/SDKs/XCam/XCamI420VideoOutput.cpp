#include "SDKs/XCam/XCamI420VideoOutput.h"

#include <cstring> // memset

// ===========================================================================
// XCAM::CI420VideoOutput -- reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamI420VideoOutput.h for the derived member
// layout and the per-method X360 addresses. `XCAM` is an X360 SDK boundary, so
// its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

namespace
{
    // Program one sampler's 2D texture-dimension bits into the X360 GPU
    // fetch-constant shadow state carried inside the D3DDevice, and flag the
    // sampler dirty. This is the inlined body of the XDK sampler-dimension setter:
    // uSamplerReg is the fetch-constant register offset (0x480/0x498/0x4B0 for
    // samplers 0/1/2) and uDirtyMask is that sampler's dirty bit
    // (0x80000000/0x40000000/0x20000000). Raw device-register access is inherent
    // platform data (opaque XDK D3DDevice), reproduced exactly as the asm stores.
    // FLAG PC-platform leaf: X360 D3D9 GPU fetch-constant shadow-state poke.
    void SetSamplerDimension2D(D3DDevice* pDevice, u32 uSamplerReg, u32 uDirtyMask)
    {
        u8* pDev = reinterpret_cast<u8*>(pDevice); // FLAG PC-platform leaf: XDK device state
        u32& rFetch = *reinterpret_cast<u32*>(pDev + uSamplerReg);
        u64& rDirty = *reinterpret_cast<u64*>(pDev + 0x18); // FLAG PC-platform leaf: sampler dirty mask

        // rlwimi r10,1,11,19,21 : insert 1 into the [12:10] dimension field.
        rFetch = (rFetch & 0xFFFFE3FFu) | 0x00000800u;
        rDirty |= static_cast<u64>(uDirtyMask);
        // rlwimi r10,1,14,16,18 : insert 1 into the [15:13] dimension field.
        rFetch = (rFetch & 0xFFFF1FFFu) | 0x00004000u;
        rDirty |= static_cast<u64>(uDirtyMask);
    }
}

// @ 0x82985DB0
int CI420VideoOutput::Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight)
{
    int iResult = CVideoOutputBase::Initialize(pDevice, iWidth, iHeight);
    if (iResult != 0)
        return iResult;

    if (pDevice)
    {
        // Device present: back each plane with a D3D texture and cache its locked
        // pitch/pointer. mpResource0C (base @0x0C) holds the YUV->RGB pixel shader.
        mpResource0C = D3DDevice_CreatePixelShader(gXCamI420PixelShaderCode);
        if (!mpResource0C)
            return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

        for (int i = 0; i < 3; ++i)
        {
            mpBufferResource[i] = D3DDevice_CreateTexture(miWidth, miHeight, 1, 1, 0,
                                                          KU_XCAM_I420_PLANE_FORMAT, 0,
                                                          KU_XCAM_I420_TEXTURE_TYPE);
            if (!mpBufferResource[i])
                return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

            mpUTexture[i] = D3DDevice_CreateTexture(miWidth >> 1, miHeight >> 1, 1, 1, 0,
                                                    KU_XCAM_I420_PLANE_FORMAT, 0,
                                                    KU_XCAM_I420_TEXTURE_TYPE);
            if (!mpUTexture[i])
                return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

            mpVTexture[i] = D3DDevice_CreateTexture(miWidth >> 1, miHeight >> 1, 1, 1, 0,
                                                    KU_XCAM_I420_PLANE_FORMAT, 0,
                                                    KU_XCAM_I420_TEXTURE_TYPE);
            if (!mpVTexture[i])
                return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

            // Lock/unlock once so each descriptor caches the driver's row pitch and
            // backing pointer (the D3DLOCKED_RECT aliases SVideoBuffer {pitch,data}).
            D3DTexture_LockRect(mpBufferResource[i], 0, &mBuffers[i], nullptr, 0);
            D3DTexture_UnlockRect(mpBufferResource[i], 0);
            D3DTexture_LockRect(mpUTexture[i], 0, &mUBuffers[i], nullptr, 0);
            D3DTexture_UnlockRect(mpUTexture[i], 0);
            D3DTexture_LockRect(mpVTexture[i], 0, &mVBuffers[i], nullptr, 0);
            D3DTexture_UnlockRect(mpVTexture[i], 0);
        }
    }
    else
    {
        // No device: back each plane with a raw XMem block (Y full-res, U/V quarter).
        for (int i = 0; i < 3; ++i)
        {
            const u32 uLumaBytes = static_cast<u32>(miWidth) * static_cast<u32>(miHeight);

            mpBufferResource[i] = static_cast<D3DResource*>(
                XMemAlloc(uLumaBytes, KU_XCAM_I420_XMEM_ALLOC_ATTRIBUTES));
            mpUTexture[i] = static_cast<D3DResource*>(
                XMemAlloc(uLumaBytes >> 2, KU_XCAM_I420_XMEM_ALLOC_ATTRIBUTES));
            mpVTexture[i] = static_cast<D3DResource*>(
                XMemAlloc(uLumaBytes >> 2, KU_XCAM_I420_XMEM_ALLOC_ATTRIBUTES));

            if (!mpBufferResource[i] || !mpUTexture[i] || !mpVTexture[i])
                return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

            mBuffers[i].mpData  = mpBufferResource[i];
            mBuffers[i].muPitch = static_cast<u32>(miWidth);
            mUBuffers[i].mpData  = mpUTexture[i];
            mUBuffers[i].muPitch = static_cast<u32>(miWidth) >> 1;
            mVBuffers[i].mpData  = mpVTexture[i];
            mVBuffers[i].muPitch = static_cast<u32>(miWidth) >> 1;
        }
    }

    Reset();
    return 0;
}

// @ 0x82985018
void CI420VideoOutput::Shutdown()
{
    if (mpDevice)
    {
        // Device path: the U/V slots hold D3D textures.
        for (int i = 0; i < 3; ++i)
        {
            if (mpUTexture[i]) { D3DResource_Release(mpUTexture[i]); mpUTexture[i] = nullptr; }
            if (mpVTexture[i]) { D3DResource_Release(mpVTexture[i]); mpVTexture[i] = nullptr; }
        }
    }
    else
    {
        // No-device path: the U/V slots hold raw XMem blocks.
        for (int i = 0; i < 3; ++i)
        {
            if (mpUTexture[i]) { XMemFree(mpUTexture[i], KU_XCAM_XMEM_FREE_ATTRIBUTES); mpUTexture[i] = nullptr; }
            if (mpVTexture[i]) { XMemFree(mpVTexture[i], KU_XCAM_XMEM_FREE_ATTRIBUTES); mpVTexture[i] = nullptr; }
        }
    }

    // Chain to the base to release the Y plane + the quad resources.
    CVideoOutputBase::Shutdown();
}

// @ 0x829850D0
void CI420VideoOutput::Render()
{
    D3DDevice* pDevice = mpDevice;
    const u32 uIndex = muReadyIndex; // the last completed frame

    D3DDevice_SetVertexDeclaration(pDevice, mpVertexDeclaration);
    D3DDevice_SetVertexShader(pDevice, mpVertexShader);
    D3DDevice_SetPixelShader(pDevice, static_cast<D3DPixelShader*>(mpResource0C));

    D3DDevice_SetTexture(pDevice, 0, mpBufferResource[uIndex], 0x80000000u); // Y
    D3DDevice_SetTexture(pDevice, 1, mpUTexture[uIndex], 0x40000000u);       // U
    D3DDevice_SetTexture(pDevice, 2, mpVTexture[uIndex], 0x20000000u);       // V
    D3DDevice_SetStreamSource(pDevice, 0, mpVertexBuffer, 0, 20, 1);

    SetSamplerDimension2D(pDevice, 0x480, 0x80000000u);
    D3DDevice_SetSamplerState_MinFilter(pDevice, 0, 1);
    D3DDevice_SetSamplerState_MagFilter(pDevice, 0, 1);

    SetSamplerDimension2D(pDevice, 0x498, 0x40000000u);
    D3DDevice_SetSamplerState_MinFilter(pDevice, 1, 1);
    D3DDevice_SetSamplerState_MagFilter(pDevice, 1, 1);

    SetSamplerDimension2D(pDevice, 0x4B0, 0x20000000u);
    D3DDevice_SetSamplerState_MinFilter(pDevice, 2, 1);
    D3DDevice_SetSamplerState_MagFilter(pDevice, 2, 1);

    D3DDevice_SetRenderState_ZEnable(pDevice, 0);
    D3DDevice_SetRenderState_CullMode(pDevice, 0);
    D3DDevice_SetRenderState_ViewportEnable(pDevice, 0);
    D3DDevice_DrawVertices(pDevice, 6, 0, 4);

    // Unbind everything.
    D3DDevice_SetVertexDeclaration(pDevice, nullptr);
    D3DDevice_SetVertexShader(pDevice, nullptr);
    D3DDevice_SetPixelShader(pDevice, nullptr);
    D3DDevice_SetTexture(pDevice, 0, nullptr, 0x80000000u);
    D3DDevice_SetTexture(pDevice, 1, nullptr, 0x40000000u);
    D3DDevice_SetTexture(pDevice, 2, nullptr, 0x20000000u);
    D3DDevice_SetStreamSource(pDevice, 0, nullptr, 0, 0, 1);
}

// @ 0x82984698
void CI420VideoOutput::Reset()
{
    mbBufferReady = 0;
    muReadyIndex  = 0;
    muWriteIndex  = 1;

    for (int i = 0; i < 3; ++i)
    {
        // Luma cleared to 0 (black); chroma to 0x80 (neutral).
        memset(mBuffers[i].mpData, 0, mBuffers[i].muPitch * static_cast<u32>(miHeight));
        memset(mUBuffers[i].mpData, 0x80, (static_cast<u32>(miHeight) >> 1) * mUBuffers[i].muPitch);
        memset(mVBuffers[i].mpData, 0x80, (static_cast<u32>(miHeight) >> 1) * mVBuffers[i].muPitch);
    }
}

// @ 0x82984790
int CI420VideoOutput::GetNextBuffer(SVideoBuffer* pY, SVideoBuffer* pU, SVideoBuffer* pV)
{
    const u32 uIndex = static_cast<u32>(GetNextBufferHelper());
    if (uIndex >= 3)
        return KI_XCAM_NO_BUFFER_READY;

    CVideoOutputBase::CopyBufferHelper(&mBuffers[uIndex], pY, miWidth, miHeight);
    CVideoOutputBase::CopyBufferHelper(&mUBuffers[uIndex], pU, miWidth >> 1, miHeight >> 1);
    CVideoOutputBase::CopyBufferHelper(&mVBuffers[uIndex], pV, miWidth >> 1, miHeight >> 1);
    return 0;
}

// @ 0x82984728
void CI420VideoOutput::GetBuffersForWriting(SVideoBuffer* pY, SVideoBuffer* pU, SVideoBuffer* pV)
{
    *pY = mBuffers[muWriteIndex];
    *pU = mUBuffers[muWriteIndex];
    *pV = mVBuffers[muWriteIndex];
}

} // namespace XCAM
