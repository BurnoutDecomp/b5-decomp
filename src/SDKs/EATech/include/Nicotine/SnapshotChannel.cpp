#include "SDKs/EATech/include/Nicotine/SnapshotChannel.hpp"

// ===========================================================================
//  Nicotine::SnapshotChannel -- volume accessors.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX. X360 asm is
//  authoritative. See SnapshotChannel.hpp for the member layout and the
//  SnapshotVolumeCurve (sub_82B453C0) dependency rationale.
//
//      Nicotine::SnapshotChannel::GetCurrentVolume  @ 0x82B47200
//      Nicotine::SnapshotChannel::GetFinalVolume    @ 0x82B472B8
//      Nicotine::SnapshotChannel::GetCeilingVolume  @ 0x82B47338
// ===========================================================================

namespace Nicotine
{

s16 SnapshotChannel::GetCurrentVolume()
{
    if (mfElapsed < mfDuration)
    {
        const f32 lfRatio = mfElapsed / mfDuration;
        int liCurve = 7;
        if (mi16BaseVolume <= mi16TargetVolume)
            liCurve = 1;

        const f32 lfBlend = SnapshotVolumeCurve(lfRatio, liCurve);
        const f32 lfDelta = static_cast<f32>(mi16TargetVolume - mi16BaseVolume);
        return static_cast<s16>(mi16BaseVolume + static_cast<s16>(lfDelta * lfBlend));
    }
    return mi16TargetVolume;
}

s16 SnapshotChannel::GetFinalVolume()
{
    if (mpCeiling)
    {
        int liVolume = static_cast<s16>(mpCeiling->GetFinalVolume() + GetCurrentVolume());
        if (liVolume <= -10000)
            liVolume = -10000;
        return static_cast<s16>(liVolume);
    }
    return GetCurrentVolume();
}

// GetCeilingVolume @ 0x82B47338
//   if (mpCeiling) return mpCeiling->GetFinalVolume();  else return 0;
s16 SnapshotChannel::GetCeilingVolume()
{
    SnapshotChannel* lpCeiling = mpCeiling;   // lwz r3, 0xC(r3)
    if (lpCeiling != 0)
    {
        return lpCeiling->GetFinalVolume();   // b GetFinalVolume (tail call, this = ceiling)
    }
    return 0;                                 // li r3, 0
}

} // namespace Nicotine
