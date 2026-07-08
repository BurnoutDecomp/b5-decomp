#include "SDKs/XCam/XCamDecoderOutput.h"

// ===========================================================================
// XCAM::CDecoderOutput -- reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamDecoderOutput.h for the derived member layout
// and the per-method X360 addresses. `XCAM` is an X360 SDK boundary, so its
// identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

namespace
{
    // Program one sampler's 2D texture-dimension bits into the X360 GPU
    // fetch-constant shadow state carried inside the D3DDevice, and flag the
    // sampler dirty -- the same inlined XDK sampler-dimension setter the I420
    // output uses. uSamplerReg is the fetch-constant register offset (0x480 for
    // sampler 0) and uDirtyMask is that sampler's dirty bit. Raw device-register
    // access is inherent platform data (opaque XDK D3DDevice), reproduced exactly
    // as the asm stores.
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

// @ 0x82986018
int CDecoderOutput::Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight)
{
    int iResult = CI420VideoOutput::Initialize(pDevice, iWidth, iHeight);
    if (iResult != 0)
        return iResult;

    mpDefaultTexture = nullptr;
    mbStreamStale    = 0;

    // Seed every liveness-FIFO slot with the current tick (oldest first).
    for (int i = KI_XCAM_FRAME_TICK_HISTORY - 1; i >= 0; --i)
        mauFrameTicks[i] = GetTickCount();

    if (pDevice)
    {
        mpRGBPixelShader = D3DDevice_CreatePixelShader(gXCamRGBPixelShaderCode);
        if (!mpRGBPixelShader)
            return KI_XCAM_VIDEO_OUT_OF_RESOURCES;
    }
    return 0;
}

// @ 0x829857D0
void CDecoderOutput::Shutdown()
{
    // Only the device path owns the fallback D3D resources.
    if (mpDevice)
    {
        if (mpDefaultTexture)
        {
            D3DResource_Release(mpDefaultTexture);
            mpDefaultTexture = nullptr;
        }
        if (mpRGBPixelShader)
        {
            D3DResource_Release(mpRGBPixelShader);
            mpRGBPixelShader = nullptr;
        }
    }

    CI420VideoOutput::Shutdown();
}

// @ 0x82985840
void CDecoderOutput::Reset()
{
    mbStreamStale = 0;

    if (mpDefaultTexture)
    {
        D3DResource_Release(mpDefaultTexture);
        mpDefaultTexture = nullptr;
    }

    CI420VideoOutput::Reset();
}

// @ 0x829858F0
void CDecoderOutput::Render()
{
    // Judge the feed stale once no frame has arrived for ~2 s.
    if (GetTickCount() - mauFrameTicks[0] > KU_XCAM_STREAM_STALE_MS)
        mbStreamStale = 1;

    // While live (or with no fallback texture set) draw the real YUV frame.
    if (!mbStreamStale || !mpDefaultTexture)
    {
        CI420VideoOutput::Render();
        return;
    }

    // Stalled: blit the caller-supplied default RGB texture on the quad instead.
    D3DDevice* pDevice = mpDevice;

    D3DDevice_SetVertexDeclaration(pDevice, mpVertexDeclaration);
    D3DDevice_SetVertexShader(pDevice, mpVertexShader);
    D3DDevice_SetPixelShader(pDevice, mpRGBPixelShader);
    D3DDevice_SetTexture(pDevice, 0, mpDefaultTexture, 0x80000000u);
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

// @ 0x82984838
void CDecoderOutput::BufferWriteSucceeded()
{
    // Publish the frame through the base ring bookkeeping first.
    CVideoOutputBase::BufferWriteSucceeded();

    // Slide the tick FIFO up one slot (oldest drops off the end).
    for (int i = KI_XCAM_FRAME_TICK_HISTORY - 1; i > 0; --i)
        mauFrameTicks[i] = mauFrameTicks[i - 1];

    const u32 uNow      = GetTickCount();
    const u32 uWasStale = mbStreamStale;
    mauFrameTicks[0] = uNow;

    // Once flagged stale, only recover when the oldest kept frame is within ~2 s
    // of now (i.e. the last few frames streamed in fast enough).
    if (uWasStale)
    {
        mbStreamStale =
            (uNow - mauFrameTicks[KI_XCAM_FRAME_TICK_HISTORY - 1]) > KU_XCAM_STREAM_STALE_MS;
    }
}

// @ 0x82985898
void CDecoderOutput::SetDefaultRGBTexture(D3DTexture* pTexture)
{
    if (mpDefaultTexture)
        D3DResource_Release(mpDefaultTexture);

    mpDefaultTexture = pTexture;

    if (pTexture)
        D3DResource_AddRef(pTexture);
}

} // namespace XCAM
