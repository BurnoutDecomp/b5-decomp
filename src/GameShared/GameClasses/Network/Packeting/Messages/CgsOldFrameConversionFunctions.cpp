#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

#include <cmath>   // fmaf, fabsf

// CgsNetwork::OLDTranslateFrame50HzTo60Hz / OLDTranslateFrame60HzTo50Hz: the float
// versions of the frame-rate translation. TranslateFrame50HzTo60Hz / 60HzTo50Hz run them
// only to cross-check their own integer result within a tolerance.
//
// A frame is turned into seconds on the sender's clock (frame * step + wraps * wrap
// time). The candidates are the current wrap and, for a frame near either end of the
// 16-bit range, the next or the previous one; the candidate nearest the receiver's
// current time wins. That time is reduced modulo the other rate's wrap time and turned
// into a frame at the other rate (rounded half up for 60 Hz, truncated for 50 Hz). The
// console evaluates every multiply-add as one fused single-precision operation, so
// they are written with fmaf.

namespace CgsNetwork
{
    namespace
    {
        const f32 KF_OLD_TIME_STEP_50HZ = 0.02f;
        const f32 KF_OLD_TIME_STEP_60HZ = 1.0f / 60.0f;
        const f32 KF_OLD_WRAP_TIME_50HZ = 1310.7f;     // 65535 frames at 50 Hz
        const f32 KF_OLD_WRAP_TIME_60HZ = 1092.25f;    // 65535 frames at 60 Hz

        // The reciprocals the console multiplies by (image values, bit-exact).
        const f32 KF_OLD_INV_WRAP_TIME_50HZ     = 0x1.900190p-11f;   // 1 / 1310.7
        const f32 KF_OLD_INV_WRAP_TIME_60HZ     = 0x1.E001E0p-11f;   // 1 / 1092.25
        const f32 KF_OLD_FRAMES_PER_SECOND_60HZ = 0x1.DFFFFEp+5f;    // 1 / KF_OLD_TIME_STEP_60HZ
        const f32 KF_OLD_FRAMES_PER_SECOND_50HZ = 50.0f;

        const f32 KF_NO_ALTERNATIVE   = -1.0f;
        const u32 KU_NEAR_WRAP_FRAMES = 0x5555;   // a third of the 16-bit range
        const s32 KI_LAST_FRAME       = 0xFFFE;

        // The time difference marks "no alternative wrap" with the largest float.
        const f32 KF_NO_ALTERNATIVE_TIME = 3.40282347e+38f;
    }

    u16 OLDTranslateFrame50HzTo60Hz(u16 lu16Frame50Hz, u16 lu16CurrentFrame50Hz, u16 lu16NumWraps)
    {
        const f32 lfFrameTime   = static_cast<f32>(static_cast<s32>(lu16Frame50Hz)) * KF_OLD_TIME_STEP_50HZ;
        const f32 lfWrapsTime   = static_cast<f32>(static_cast<s32>(lu16NumWraps)) * KF_OLD_WRAP_TIME_50HZ;
        const f32 lfCurrentTime = fmaf(static_cast<f32>(lu16CurrentFrame50Hz), KF_OLD_TIME_STEP_50HZ, lfWrapsTime);
        const f32 lfTime        = lfFrameTime + lfWrapsTime;

        f32 lfAlternativeTime = KF_NO_ALTERNATIVE;
        if (lu16Frame50Hz < KU_NEAR_WRAP_FRAMES)
        {
            lfAlternativeTime = fmaf(static_cast<f32>(static_cast<s32>(lu16NumWraps) + 1),
                                     KF_OLD_WRAP_TIME_50HZ, lfFrameTime);
        }
        if (KI_LAST_FRAME - static_cast<s32>(lu16Frame50Hz) < static_cast<s32>(KU_NEAR_WRAP_FRAMES))
        {
            lfAlternativeTime = fmaf(static_cast<f32>(static_cast<s32>(lu16NumWraps) - 1),
                                     KF_OLD_WRAP_TIME_50HZ, lfFrameTime);
        }

        f32 lfChosenTime = lfAlternativeTime;
        if (lfAlternativeTime == KF_NO_ALTERNATIVE
            || fabsf(lfCurrentTime - lfTime) < fabsf(lfCurrentTime - lfAlternativeTime))
        {
            lfChosenTime = lfTime;
        }

        const f32 lfWholeWraps = static_cast<f32>(static_cast<s32>(lfChosenTime * KF_OLD_INV_WRAP_TIME_60HZ));
        const f32 lfTime60Hz   = fmaf(-lfWholeWraps, KF_OLD_WRAP_TIME_60HZ, lfChosenTime);
        return static_cast<u16>(static_cast<s64>(fmaf(lfTime60Hz, KF_OLD_FRAMES_PER_SECOND_60HZ, 0.5f)));
    }

    u16 OLDTranslateFrame60HzTo50Hz(u16 lu16Frame60Hz, u16 lu16CurrentFrame60Hz, u16 lu16NumWraps)
    {
        const f32 lfFrameTime   = static_cast<f32>(static_cast<s32>(lu16Frame60Hz)) * KF_OLD_TIME_STEP_60HZ;
        const f32 lfWrapsTime   = static_cast<f32>(static_cast<s32>(lu16NumWraps)) * KF_OLD_WRAP_TIME_60HZ;
        const f32 lfCurrentTime = fmaf(static_cast<f32>(lu16CurrentFrame60Hz), KF_OLD_TIME_STEP_60HZ, lfWrapsTime);
        const f32 lfTime        = lfFrameTime + lfWrapsTime;

        f32 lfAlternativeTime = KF_NO_ALTERNATIVE;
        if (lu16Frame60Hz < KU_NEAR_WRAP_FRAMES)
        {
            lfAlternativeTime = fmaf(static_cast<f32>(static_cast<s32>(lu16NumWraps) + 1),
                                     KF_OLD_WRAP_TIME_60HZ, lfFrameTime);
        }
        if (KI_LAST_FRAME - static_cast<s32>(lu16Frame60Hz) < static_cast<s32>(KU_NEAR_WRAP_FRAMES))
        {
            lfAlternativeTime = fmaf(static_cast<f32>(static_cast<s32>(lu16NumWraps) - 1),
                                     KF_OLD_WRAP_TIME_60HZ, lfFrameTime);
        }

        f32 lfChosenTime = lfAlternativeTime;
        if (lfAlternativeTime == KF_NO_ALTERNATIVE
            || fabsf(lfCurrentTime - lfTime) < fabsf(lfCurrentTime - lfAlternativeTime))
        {
            lfChosenTime = lfTime;
        }

        const f32 lfWholeWraps = static_cast<f32>(static_cast<s32>(lfChosenTime * KF_OLD_INV_WRAP_TIME_50HZ));
        const f32 lfTime50Hz   = fmaf(-lfWholeWraps, KF_OLD_WRAP_TIME_50HZ, lfChosenTime);
        return static_cast<u16>(static_cast<s64>(lfTime50Hz * KF_OLD_FRAMES_PER_SECOND_50HZ));
    }

    // The float version of GetTimeDiffWrapped16, which runs it only to cross-check its own
    // integer result. Frame A becomes seconds on its own clock (frame * step + wraps * wrap
    // time); frame B is placed in the wrap of B's clock that time falls in and, for a frame
    // near either end of the 16-bit range, also in the next or the previous wrap. The
    // smaller of the two differences wins.
    f32 OLDGetTimeDiffWrapped16(u16 lu16FramesA, u16 lu16FramesB, s32 liNumWraps,
                                bool lbFramesAAre50Hz, bool lbFramesBAre50Hz)
    {
        const f32 lfWrapTimeA = lbFramesAAre50Hz ? KF_OLD_WRAP_TIME_50HZ : KF_OLD_WRAP_TIME_60HZ;
        const f32 lfTimeStepA = lbFramesAAre50Hz ? KF_OLD_TIME_STEP_50HZ : KF_OLD_TIME_STEP_60HZ;
        const f32 lfWrapTimeB = lbFramesBAre50Hz ? KF_OLD_WRAP_TIME_50HZ : KF_OLD_WRAP_TIME_60HZ;
        const f32 lfTimeStepB = lbFramesBAre50Hz ? KF_OLD_TIME_STEP_50HZ : KF_OLD_TIME_STEP_60HZ;

        const f32 lfTimeA = fmaf(static_cast<f32>(lu16FramesA), lfTimeStepA,
                                 static_cast<f32>(liNumWraps) * lfWrapTimeA);
        const f32 lfFrameTimeB = static_cast<f32>(lu16FramesB) * lfTimeStepB;

        const s32 liNumWrapsB = static_cast<s32>(lfTimeA / lfWrapTimeB);
        const f32 lfTimeDiff  = lfTimeA - fmaf(static_cast<f32>(liNumWrapsB), lfWrapTimeB, lfFrameTimeB);

        f32 lfAlternativeTimeB    = KF_NO_ALTERNATIVE_TIME;
        f32 lfAlternativeTimeDiff = KF_NO_ALTERNATIVE_TIME;
        if (lu16FramesB < KU_NEAR_WRAP_FRAMES)
        {
            lfAlternativeTimeB    = fmaf(static_cast<f32>(liNumWrapsB + 1), lfWrapTimeB, lfFrameTimeB);
            lfAlternativeTimeDiff = lfTimeA - lfAlternativeTimeB;
        }
        if (KI_LAST_FRAME - static_cast<s32>(lu16FramesB) < static_cast<s32>(KU_NEAR_WRAP_FRAMES))
        {
            lfAlternativeTimeB    = fmaf(static_cast<f32>(liNumWrapsB - 1), lfWrapTimeB, lfFrameTimeB);
            lfAlternativeTimeDiff = lfTimeA - lfAlternativeTimeB;
        }

        if (lfAlternativeTimeB == KF_NO_ALTERNATIVE_TIME
            || fabsf(lfTimeDiff) < fabsf(lfAlternativeTimeDiff))
        {
            return lfTimeDiff;
        }
        return lfAlternativeTimeDiff;
    }
}
