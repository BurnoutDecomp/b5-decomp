#include "SDKs/XCam/XCamEncoder.h"

#include <cstring> // memset
#include <intrin.h> // _InterlockedIncrement -- portable stand-in for the X360
                    // interrupt-masked lwarx/stwcx. reservation increment.

// ===========================================================================
// XCAM::CEncoder -- reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
// See XCamEncoder.h for the member layout, the EMB SDK / platform prototypes and
// the per-method X360 addresses. `XCAM`/`EMB` are X360 SDK boundaries, so their
// identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

// @ 0x82983E78
int CEncoder::Initialize(u32 uDimensions, u32 uFramerate, u32 uBitrate, int iInitParam,
                         int bEnableVideoOutput, D3DDevice* pDevice)
{
    // The dimensions arrive packed: width in the high half, height in the low.
    const s32 iWidth  = static_cast<s32>(uDimensions >> 16);
    const s32 iHeight = static_cast<s32>(uDimensions & 0xFFFF);

    mbHasVideoOutput = bEnableVideoOutput;

    // Seed both the target and the last-applied rate-control values so EncodeFrame
    // does not fire a spurious change on the very first frame.
    miFramerate             = static_cast<s32>(uFramerate);
    miBitrate               = static_cast<s32>(uBitrate);
    miAppliedFramerate      = static_cast<s32>(uFramerate);
    miAppliedBitrate        = static_cast<s32>(uBitrate);
    mbRecoveryFrameRequired = 0;
    muLastEncodeTick        = 0;

    mpEncSession = EMBEncSessionCreate(KU_XCAM_ENC_CODEC_WVC1);
    if (!mpEncSession)
        return KI_XCAM_ENCODER_INIT_FAILED;

    EMBEncSessionSetRTC(mpEncSession);

    // Configure the session for the YUY2 source at the requested dimensions and
    // rate-control targets. The remaining arguments are the encoder's fixed RTC
    // configuration constants baked into the X360 image; the exact EMB prototype
    // is unavailable, so this call is reconstructed from the X360 call site.
    if (EMBEncInit(mpEncSession, KI_XCAM_ENC_PROFILE, iWidth, iHeight,
                   KI_XCAM_ENC_MAX_BITRATE, KI_XCAM_ENC_LATENCY_MS,
                   KI_XCAM_ENC_GOP_LENGTH, KI_XCAM_ENC_VBV_SIZE, &iInitParam,
                   static_cast<double>(static_cast<s32>(uFramerate)),
                   static_cast<double>(uBitrate) * 0.9))
    {
        return KI_XCAM_ENCODER_INIT_FAILED;
    }

    // Probe the encoder's extended format description.
    muExtendedFormatSize = KU_XCAM_EXT_FORMAT_SIZE;
    if (EMBEncGetExtendedFormat(mpEncSession, mExtendedFormat, &muExtendedFormatSize))
        return KI_XCAM_ENCODER_INIT_FAILED;

    // Clear the double-buffered request slots and arm the guarding lock.
    memset(mOverlappedIO, 0, sizeof(mOverlappedIO));
    muCurrentIndex = 0;
    RtlInitializeCriticalSectionAndSpinCount(&mCriticalSection, KU_XCAM_CS_SPIN_COUNT);

    // Build the BITMAPINFOHEADER for the YUY2 encoded frames.
    memset(&mFormat, 0, sizeof(mFormat));
    mFormat.miWidth      = iWidth;
    mFormat.miHeight     = iHeight;
    muFrameCounter       = 0;
    mFormat.muSize       = KU_XCAM_BMIH_SIZE;
    mFormat.muBitCount   = KU_XCAM_YUY2_BITCOUNT;
    mFormat.muCompression = KU_XCAM_FOURCC_YUY2;
    mFormat.muSizeImage  = iWidth * iHeight * 2; // 16bpp YUY2 -> 2 bytes per pixel

    if (mbHasVideoOutput)
        return mVideoOutput.Initialize(pDevice, iWidth, iHeight);

    return 0;
}

// @ 0x82984040
void CEncoder::Shutdown()
{
    if (mpEncSession)
        EMBEncSessionDelete(mpEncSession);

    mVideoOutput.CVideoOutputBase::Shutdown();

    // Complete any request still pending in either double-buffer slot.
    if (mOverlappedIO[0].GetOverlapped())
        mOverlappedIO[0].SignalComplete(KU_XCAM_SIGNAL_STATUS_TEARDOWN,
                                        KU_XCAM_SIGNAL_ERROR_TEARDOWN, 0);
    if (mOverlappedIO[1].GetOverlapped())
        mOverlappedIO[1].SignalComplete(KU_XCAM_SIGNAL_STATUS_TEARDOWN,
                                        KU_XCAM_SIGNAL_ERROR_TEARDOWN, 0);
}

// @ 0x829840C0
u32 CEncoder::SubmitFrameToEncodeQueue(u32 uFrameData, XOVERLAPPED* pOverlapped)
{
    if (mbHasVideoOutput)
    {
        // Copy the raw camera frame into the YUY2 output's current write buffer.
        SVideoBuffer srcBuffer;
        srcBuffer.muPitch = 2 * static_cast<u32>(mFormat.miWidth);
        srcBuffer.mpData  = reinterpret_cast<void*>(static_cast<uintptr_t>(uFrameData));

        SVideoBuffer dstBuffer;
        mVideoOutput.GetBufferForWriting(&dstBuffer);
        CVideoOutputBase::CopyBufferHelper(&srcBuffer, &dstBuffer,
                                           2 * static_cast<u32>(mFormat.miWidth),
                                           static_cast<u32>(mFormat.miHeight));
        mVideoOutput.BufferWriteSucceeded();
    }

    // Carry the frame pointer to the encoder worker through the overlapped.
    pOverlapped->InternalContext = uFrameData;

    RtlEnterCriticalSection(&mCriticalSection);

    // Enqueue into the slot the encoder worker is NOT currently draining.
    COverlappedIO* pSlot = &mOverlappedIO[1 - muCurrentIndex];
    if (pSlot->GetOverlapped())
        pSlot->SignalComplete(0, 0, 0);
    u32 uResult = pSlot->Setup(pOverlapped);

    RtlLeaveCriticalSection(&mCriticalSection);
    return uResult;
}

// @ 0x82984188
int CEncoder::EncodeFrame(int iFrameKind, int iParam, u32* pOutStatus,
                          u32* pbIsIntraFrame, void* pRecoveryFrameType)
{
    // Flip the double-buffer under the lock.
    RtlEnterCriticalSection(&mCriticalSection);
    muCurrentIndex = static_cast<u8>(1 - muCurrentIndex);
    RtlLeaveCriticalSection(&mCriticalSection);

    COverlappedIO* pSlot = &mOverlappedIO[muCurrentIndex];
    if (!pSlot->GetOverlapped())
    {
        *pOutStatus = 0;
        return KI_XCAM_ENCODER_NO_REQUEST;
    }

    muLastEncodeTick = GetTickCount();

    // Apply any pending rate-control changes lazily, once per encode.
    if (mbRecoveryFrameRequired)
    {
        mbRecoveryFrameRequired = 0;
        EMBEncRTCSetNetworkMetrics(mpEncSession, 0, 0.5);
    }
    if (miAppliedBitrate != miBitrate)
    {
        miAppliedBitrate = miBitrate;
        EMBEncRTCChangeBitRate(mpEncSession,
                               static_cast<double>(static_cast<u32>(miBitrate)) * 0.9);
    }
    if (miAppliedFramerate != miFramerate)
    {
        miAppliedFramerate = miFramerate;
        EMBEncRTCChangeFrameRate(mpEncSession, static_cast<double>(miFramerate));
    }

    const u8 bForceKeyFrame = (iFrameKind == 1) ? 1u : 0u;

    u8 bFrameDropped = 0;
    int iEncodeStatus = EMBEncEncodeAndLock(mpEncSession, &mFormat,
                                            pSlot->GetOverlapped()->InternalContext,
                                            iParam, pOutStatus,
                                            mFormat.miWidth, mFormat.miHeight,
                                            bForceKeyFrame, &bFrameDropped);
    if (bFrameDropped || iEncodeStatus)
    {
        *pOutStatus = 0;
        pSlot->SignalComplete(0, 0, 0);
        return KI_XCAM_ENCODER_FRAME_DROPPED;
    }

    if (pbIsIntraFrame)
    {
        s64 iPredType = 0;
        EMBEncGetPredType(mpEncSession, &iPredType);
        *pbIsIntraFrame = (static_cast<u32>(iPredType >> 32) == 0) ? 1u : 0u;
    }
    if (pRecoveryFrameType)
        EMBQueryErrorRecoveryFrameType(mpEncSession, pRecoveryFrameType);

    pSlot->SignalComplete(0, 0, mFormat.muSizeImage);

    // Interrupt-masked reservation increment of the encoded-frame counter.
    _InterlockedIncrement(reinterpret_cast<volatile long*>(&muFrameCounter));
    return 0;
}

// @ 0x829843D0
COverlappedIO* CEncoder::SkipFrame()
{
    RtlEnterCriticalSection(&mCriticalSection);
    muCurrentIndex = static_cast<u8>(1 - muCurrentIndex);
    RtlLeaveCriticalSection(&mCriticalSection);

    COverlappedIO* pSlot = &mOverlappedIO[muCurrentIndex];
    if (pSlot->GetOverlapped())
        pSlot->SignalComplete(0, 0, 0);
    return pSlot;
}
} // namespace XCAM
