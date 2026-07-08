#include "SDKs/XCam/XCamQOSController.h"

#include <cstring>  // memcpy

// ===========================================================================
// XCAM::CQOSController -- QOS / rate-control governor, reconstructed store-for-
// store from BURNOUT_X360_ARTIST.XEX. See XCamQOSController.h for the layout and
// the split of grounded vs. blocked bodies.
//
// Only the seven bodies whose logic is fully grounded in `this` (plus GetTickCount
// and the outbound-byte atomic) are defined here. The seven that reach through
// mpEngine (XCAM::CStreamEngine) into its embedded encoder / packetizer / remote-
// console list -- or need the un-recovered frame-rate ladder rodata -- are only
// declared, and land when CStreamEngine is homed.
//
// The X360 image folds the outbound byte counter into an interrupt-disabled
// lwarx/stwcx sequence (mfmsr/mtmsree wrap); that is an atomic exchange / fetch-
// add, reconstructed as InterlockedExchange / InterlockedExchangeAdd.
// ===========================================================================

namespace XCAM
{

// @ 0x829836A0
u32 CQOSController::CalculateRealBandwidthUsage()
{
    const u32 uNow = GetTickCount();

    // Elapsed since the last measurement, as an unsigned 32-bit ms count.
    const f32 fElapsedMs = static_cast<f32>(uNow - muBandwidthTick);
    if (fElapsedMs > KF_QOS_BANDWIDTH_WINDOW_MS)
    {
        muBandwidthTick = uNow;
        // Atomically drain the byte accumulator to zero and take its old value.
        const s32 iBytes = InterlockedExchange(&miByteCounter, 0);
        mfBandwidth = (static_cast<f32>(iBytes) / fElapsedMs) * KF_QOS_BITS_PER_BYTE;
    }

    mfBandwidthRatio = mfBandwidth / static_cast<f32>(miCurrentBitrate);
    return uNow;
}

// @ 0x82983C78
u32 CQOSController::DoWork()
{
    const u32 uNow = GetTickCount();
    u32 uBitrate = miCurrentBitrate;
    u32 uResult = CalculateRealBandwidthUsage();

    if (mbKeyFrameRequested && (uNow - muKeyFrameTick) > KU_QOS_KEYFRAME_INTERVAL_MS)
    {
        mbKeyFramePending = 1;
        mbKeyFrameRequested = 0;
        uResult = GetTickCount();
        muKeyFrameTick = uResult;
    }

    if (mbRecoveryRequested && (uNow - muRecoveryTick) > KU_QOS_RECOVERY_INTERVAL_MS)
        uResult = ForceRecoveryFrame();

    if (mbDecreaseRequested && (uNow - muBitrateTick) > KU_QOS_DECREASE_INTERVAL_MS)
    {
        uResult = DecreaseBitrate();
        uBitrate = uResult;
    }
    else if ((uNow - muBitrateTick) > KU_QOS_INCREASE_INTERVAL_MS
             && miCurrentBitrate < miTargetBitrate)
    {
        uResult = IncreaseBitrate();
        uBitrate = uResult;
    }

    if (miCurrentBitrate != uBitrate)
        uResult = static_cast<u32>(SetActualBitrate(uBitrate));
    return uResult;
}

// @ 0x82983628
u32 CQOSController::ForceKeyFrame()
{
    mbKeyFramePending = 1;
    mbKeyFrameRequested = 0;
    const u32 uNow = GetTickCount();
    muKeyFrameTick = uNow;
    return uNow;
}

// @ 0x829835A8
void CQOSController::ProcessQOSFeedback(const u8* pHeader, const void* pReport)
{
    const u8 uFlags = pHeader[1];

    // Key-frame request bits fold into mbKeyFrameRequested.
    mbKeyFrameRequested |= (uFlags & KU_QOS_FLAG_KEYFRAME_A) != 0;
    mbKeyFrameRequested |= (uFlags & KU_QOS_FLAG_KEYFRAME_B) != 0;

    // Throttle-down bits fold into mbDecreaseRequested.
    mbDecreaseRequested |= (uFlags & KU_QOS_FLAG_THROTTLE_A) != 0;
    mbDecreaseRequested |= (uFlags & KU_QOS_FLAG_THROTTLE_B) != 0;

    if ((uFlags & KU_QOS_FLAG_THROTTLE_A) != 0 || (uFlags & KU_QOS_FLAG_THROTTLE_B) != 0)
        memcpy(mQOSReport, pReport, KU_QOS_REPORT_SIZE);
}

// @ 0x82983DC8
void CQOSController::SetTargetBitrate(u32 uBitrate)
{
    if (miTargetBitrate == uBitrate)
        return;

    const u32 uCurrent = miCurrentBitrate;
    miTargetBitrate = uBitrate;
    if (uCurrent > uBitrate)
        SetActualBitrate(uBitrate);
}

// @ 0x82983BF8
void CQOSController::SetTargetFramerate(int iFramerate)
{
    if (miTargetFramerate == iFramerate)
        return;

    miTargetFramerate = iFramerate;
    ResetActualFramerate();
}

// @ 0x82983668
void CQOSController::SumSendSize(u32 uSendSize, s32 iBytes)
{
    if (uSendSize > miPeakSendSize)
    {
        miPeakSendSize = uSendSize;
        // Atomically fold the byte count into the outbound accumulator.
        InterlockedExchangeAdd(&miByteCounter, iBytes);
    }
}

} // namespace XCAM
