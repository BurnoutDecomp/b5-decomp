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
            D3DTexture_LockRect(mpBufferResource[i], 0, &mBuffers[i], 0, 0);
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
    D3DDevice_SetVertexDeclaration(pDevice, 0);
    D3DDevice_SetVertexShader(pDevice, 0);
    D3DDevice_SetPixelShader(pDevice, 0);
    D3DDevice_SetTexture(pDevice, 0, 0, 0x80000000u);
    D3DDevice_SetStreamSource(pDevice, 0, 0, 0, 0, 1);
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
