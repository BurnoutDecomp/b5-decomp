#include "SDKs/XCam/XCamYUY2VideoOutput.h"

// Reuse the sibling I420 output's platform-boundary declarations (the shared
// X360 D3D9 XDK entry points -- D3DDevice_CreatePixelShader / CreateTexture /
// Texture_LockRect / Texture_UnlockRect / XMemAlloc / the SetVertex*/SetTexture/
// SetSamplerState/SetRenderState/DrawVertices setters -- plus the D3DPixelShader
// handle type and the KI_XCAM_NO_BUFFER_READY status). Both derived outputs bind
// to the same XDK boundary; homing those declarations once in the I420 header and
// including it here avoids re-declaring (forking) the platform types.
#include "SDKs/XCam/XCamI420VideoOutput.h"

// ===========================================================================
// XCAM::CYUY2VideoOutput -- reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamYUY2VideoOutput.h for the (base-only) member
// layout and the per-method X360 addresses. Unlike the planar CI420VideoOutput
// this output carries a SINGLE packed YUV 4:2:2 plane, so every plane operation
// reuses the base ring (mpBufferResource / mBuffers) with a 2-bytes-per-pixel
// stride. `XCAM` is an X360 SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

namespace
{
    // Program sampler 0's 2D texture-dimension bits into the X360 GPU
    // fetch-constant shadow state carried inside the D3DDevice, and flag the
    // sampler dirty. This is the inlined body of the XDK sampler-dimension setter
    // (identical to the CI420VideoOutput copy the X360 compiler inlined at each
    // call site): uSamplerReg is the fetch-constant register offset (0x480 for
    // sampler 0) and uDirtyMask is that sampler's dirty bit (0x80000000). Raw
    // device-register access is inherent platform data (opaque XDK D3DDevice),
    // reproduced exactly as the asm stores.
    // FLAG PC-platform leaf: X360 D3D9 GPU fetch-constant shadow-state poke.
    void SetSamplerDimension2D(D3DDevice* pDevice, u32 uSamplerReg, u32 uDirtyMask)
    {
        u8* pDev = reinterpret_cast<u8*>(pDevice); // FLAG PC-platform leaf: XDK device state
        u32& rFetch = *reinterpret_cast<u32*>(pDev + uSamplerReg);
        u64& rDirty = *reinterpret_cast<u64*>(pDev + 0x18); // FLAG PC-platform leaf: sampler dirty mask

        // rlwimi r9,1,11,19,21 : insert 1 into the [12:10] dimension field.
        rFetch = (rFetch & 0xFFFFE3FFu) | 0x00000800u;
        rDirty |= static_cast<u64>(uDirtyMask);
        // rlwimi r9,1,14,16,18 : insert 1 into the [15:13] dimension field.
        rFetch = (rFetch & 0xFFFF1FFFu) | 0x00004000u;
        rDirty |= static_cast<u64>(uDirtyMask);
    }
}

// @ 0x82985C80
int CYUY2VideoOutput::Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight)
{
    int iResult = CVideoOutputBase::Initialize(pDevice, iWidth, iHeight);
    if (iResult != 0)
        return iResult;

    if (pDevice)
    {
        // Device present: back the single packed plane with a D3D texture per ring
        // slot and cache its locked pitch/pointer. mpResource0C (base @0x0C) holds
        // the YUY2->RGB pixel shader.
        mpResource0C = D3DDevice_CreatePixelShader(gXCamYUY2PixelShaderCode);
        if (!mpResource0C)
            return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

        for (int i = 0; i < 3; ++i)
        {
            mpBufferResource[i] = D3DDevice_CreateTexture(miWidth, miHeight, 1, 1, 0,
                                                          KU_XCAM_YUY2_PLANE_FORMAT, 0,
                                                          KU_XCAM_YUY2_TEXTURE_TYPE);
            if (!mpBufferResource[i])
                return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

            // Lock/unlock once so the descriptor caches the driver's row pitch and
            // backing pointer (the D3DLOCKED_RECT aliases SVideoBuffer {pitch,data}).
            D3DTexture_LockRect(mpBufferResource[i], 0, &mBuffers[i], nullptr, 0);
            D3DTexture_UnlockRect(mpBufferResource[i], 0);
        }
    }
    else
    {
        // No device: back the packed plane with a raw XMem block (2 bytes/pixel).
        for (int i = 0; i < 3; ++i)
        {
            const u32 uBytes = 2u * static_cast<u32>(miHeight) * static_cast<u32>(miWidth);

            mpBufferResource[i] = static_cast<D3DResource*>(
                XMemAlloc(uBytes, KU_XCAM_XMEM_FREE_ATTRIBUTES));
            if (!mpBufferResource[i])
                return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

            mBuffers[i].mpData  = mpBufferResource[i];
            mBuffers[i].muPitch = 2u * static_cast<u32>(miWidth);
        }
    }

    Reset();
    return 0;
}

// @ 0x82984A50
void CYUY2VideoOutput::Render()
{
    D3DDevice* pDevice = mpDevice;
    const u32 uIndex = muReadyIndex; // the last completed frame

    D3DDevice_SetVertexDeclaration(pDevice, mpVertexDeclaration);
    D3DDevice_SetVertexShader(pDevice, mpVertexShader);
    D3DDevice_SetPixelShader(pDevice, static_cast<D3DPixelShader*>(mpResource0C));

    D3DDevice_SetTexture(pDevice, 0, mpBufferResource[uIndex], 0x80000000u);
    D3DDevice_SetStreamSource(pDevice, 0, mpVertexBuffer, 0, 20, 1);

    SetSamplerDimension2D(pDevice, 0x480, 0x80000000u);
    D3DDevice_SetSamplerState_MinFilter(pDevice, 0, 1);
    D3DDevice_SetSamplerState_MagFilter(pDevice, 0, 1);

    D3DDevice_SetRenderState_ZEnable(pDevice, 0);
    D3DDevice_SetRenderState_CullMode(pDevice, 0);
    D3DDevice_SetRenderState_ViewportEnable(pDevice, 0);
    D3DDevice_DrawVertices(pDevice, 6, 0, 4);

    // Unbind everything.
    D3DDevice_SetVertexDeclaration(pDevice, nullptr);
    D3DDevice_SetVertexShader(pDevice, nullptr);
    D3DDevice_SetPixelShader(pDevice, nullptr);
    D3DDevice_SetTexture(pDevice, 0, nullptr, 0x80000000u);
    D3DDevice_SetStreamSource(pDevice, 0, nullptr, 0, 0, 1);
}

// @ 0x82984628
int CYUY2VideoOutput::GetNextBuffer(SVideoBuffer* pDst)
{
    const u32 uIndex = static_cast<u32>(GetNextBufferHelper());
    if (uIndex >= 3)
        return KI_XCAM_NO_BUFFER_READY;

    // Packed YUY2 is 2 bytes per pixel, so a source row is 2*miWidth bytes tall by
    // miHeight rows.
    CVideoOutputBase::CopyBufferHelper(&mBuffers[uIndex], pDst, 2u * static_cast<u32>(miWidth),
                                       static_cast<u32>(miHeight));
    return 0;
}

} // namespace XCAM
