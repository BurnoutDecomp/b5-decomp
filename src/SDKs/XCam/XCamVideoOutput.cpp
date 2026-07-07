#include "SDKs/XCam/XCamVideoOutput.h"

#include <cstring> // memcpy

// ===========================================================================
// XCAM::CVideoOutputBase -- reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamVideoOutput.h for the member layout and the
// per-method X360 addresses. `XCAM` is an X360 SDK boundary, so its identifiers
// are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

// @ 0x82985AA0
int CVideoOutputBase::Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight)
{
    miWidth  = iWidth;
    miHeight = iHeight;

    if (pDevice)
    {
        mpDevice = pDevice;
        D3DDevice_AddRef(pDevice);

        mpVertexDeclaration = D3DDevice_CreateVertexDeclaration(gXCamVideoVertexElements);
        if (!mpVertexDeclaration)
            return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

        mpVertexShader = D3DDevice_CreateVertexShader(gXCamVideoVertexShaderCode);
        if (!mpVertexShader)
            return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

        mpVertexBuffer = D3DDevice_CreateVertexBuffer(80, 0, 0);
        if (!mpVertexBuffer)
            return KI_XCAM_VIDEO_OUT_OF_RESOURCES;

        // Seed the static texture coordinates for the quad's four corners:
        //   (0,0) (1,0) (0,1) (1,1). SetRenderRect later fills in the positions.
        mVertices[0].fU = 0.0f; mVertices[0].fV = 0.0f;
        mVertices[1].fU = 1.0f; mVertices[1].fV = 0.0f;
        mVertices[2].fU = 0.0f; mVertices[2].fV = 1.0f;
        mVertices[3].fU = 1.0f; mVertices[3].fV = 1.0f;

        UpdateVertexBuffer();
    }

    mSpinLock = 0;
    return 0;
}

// @ 0x829848B8
void CVideoOutputBase::Shutdown()
{
    if (mpDevice)
    {
        D3DDevice_Release(mpDevice);
        mpDevice = nullptr;

        if (mpVertexDeclaration) { D3DResource_Release(mpVertexDeclaration); mpVertexDeclaration = nullptr; }
        if (mpVertexShader)      { D3DResource_Release(mpVertexShader);      mpVertexShader      = nullptr; }
        if (mpResource0C)        { D3DResource_Release(mpResource0C);        mpResource0C        = nullptr; }
        if (mpVertexBuffer)      { D3DResource_Release(mpVertexBuffer);      mpVertexBuffer      = nullptr; }

        for (int i = 0; i < 3; ++i)
        {
            if (mpBufferResource[i])
            {
                D3DResource_Release(mpBufferResource[i]);
                mpBufferResource[i] = nullptr;
            }
        }
    }
    else
    {
        // No device was bound, so the ring blocks are raw XMem allocations.
        for (int i = 0; i < 3; ++i)
        {
            if (mpBufferResource[i])
            {
                XMemFree(mpBufferResource[i], KU_XCAM_XMEM_FREE_ATTRIBUTES);
                mpBufferResource[i] = nullptr;
            }
        }
    }
}

// @ 0x82985B80
void CVideoOutputBase::SetRenderRect(const SVideoRect* pRect)
{
    mRenderRect.iLeft   = pRect->iLeft;
    mRenderRect.iTop    = pRect->iTop;
    mRenderRect.iRight  = pRect->iRight;
    mRenderRect.iBottom = pRect->iBottom;

    // Project the integer rect onto the four quad corners (TL, TR, BL, BR).
    mVertices[0].fX = static_cast<f32>(pRect->iLeft);
    mVertices[0].fY = static_cast<f32>(pRect->iTop);
    mVertices[1].fX = static_cast<f32>(pRect->iRight);
    mVertices[1].fY = static_cast<f32>(pRect->iTop);
    mVertices[2].fX = static_cast<f32>(pRect->iLeft);
    mVertices[2].fY = static_cast<f32>(pRect->iBottom);
    mVertices[3].fX = static_cast<f32>(pRect->iRight);
    mVertices[3].fY = static_cast<f32>(pRect->iBottom);

    UpdateVertexBuffer();
}

// @ 0x82984998
void CVideoOutputBase::UpdateVertexBuffer()
{
    // Snapshot and detach the current stream 0 source so the quad's buffer can
    // be locked, then restore it afterwards.
    u32 uOffsetInBytes = 0;
    u32 uStride        = 0;
    D3DVertexBuffer* pPrevStream = D3DDevice_GetStreamSource(mpDevice, 0, &uOffsetInBytes, &uStride);
    D3DDevice_SetStreamSource(mpDevice, 0, nullptr, 0, 0, 1);

    void* pLocked = D3DVertexBuffer_Lock(mpVertexBuffer, 0, 0, 0);
    memcpy(pLocked, mVertices, 80);
    D3DVertexBuffer_Unlock(mpVertexBuffer);

    D3DDevice_SetStreamSource(mpDevice, 0, pPrevStream, uOffsetInBytes, uStride, 1);
    if (pPrevStream)
        D3DResource_Release(pPrevStream);
}

// @ 0x82984450
void CVideoOutputBase::GetBufferForWriting(SVideoBuffer* pOut)
{
    const SVideoBuffer& rBuffer = mBuffers[muWriteIndex];
    pOut->muPitch = rBuffer.muPitch;
    pOut->mpData  = rBuffer.mpData;
}

// @ 0x82984478
void CVideoOutputBase::BufferWriteSucceeded()
{
    KIRQL uOldIrql = KfAcquireSpinLock(&mSpinLock);

    u32 uIndex = muWriteIndex;
    mbBufferReady = 1;
    muReadyIndex  = uIndex;
    muWriteIndex  = uIndex + 1;
    if (uIndex + 1 >= 3)
        muWriteIndex = 0;

    KfReleaseSpinLock(&mSpinLock, uOldIrql);
}

// @ 0x829844E8
int CVideoOutputBase::GetNextBufferHelper()
{
    int iIndex = 3; // sentinel: no completed buffer
    if (mbBufferReady)
    {
        KIRQL uOldIrql = KfAcquireSpinLock(&mSpinLock);
        iIndex = static_cast<int>(muReadyIndex);
        mbBufferReady = 0;
        KfReleaseSpinLock(&mSpinLock, uOldIrql);
    }
    return iIndex;
}

// @ 0x82984538
void* CVideoOutputBase::CopyBufferHelper(SVideoBuffer* pSrc, SVideoBuffer* pDst,
                                         u32 uRowSize, u32 uRowCount)
{
    u8* pSrcData = static_cast<u8*>(pSrc->mpData);
    u8* pDstData = static_cast<u8*>(pDst->mpData);
    void* pResult = pSrc; // r3 pass-through when no copy runs (pitch mismatch, 0 rows)

    if (pSrc->muPitch == pDst->muPitch)
    {
        // Contiguous: pitches match, so copy the whole span in one go.
        pResult = memcpy(pDstData, pSrcData, pSrc->muPitch * uRowCount);
    }
    else if (uRowCount != 0)
    {
        // Pitch mismatch: copy row by row, striding by each buffer's own pitch.
        u32 uRemaining = uRowCount;
        do
        {
            pResult = memcpy(pDstData, pSrcData, uRowSize);
            --uRemaining;
            pSrcData += pSrc->muPitch;
            pDstData += pDst->muPitch;
        }
        while (uRemaining != 0);
    }

    return pResult;
}

} // namespace XCAM
