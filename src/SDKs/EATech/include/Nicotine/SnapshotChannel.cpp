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

// GetCurrentVolume @ 0x82B47200
//   f0 = mfElapsed; f13 = mfDuration;
//   if (f0 >= f13) return (s16)mi16TargetVolume;          // ramp complete
//   f1 = f0 / f13;                                        // normalised position
//   base = (s16)mi16BaseVolume; target = (s16)mi16TargetVolume;
//   curve = (base <= target) ? 1 : 7;                     // li r4,7 default; li r4,1 when base<=target
//   blend = SnapshotVolumeCurve(f1, curve);
//   // delta = target - base; product = trunc((double)delta * blend);  (fcfid/fmuls/fctiwz)
//   return (s16)( base + (low16 of product) );            // add r3, r10, r11
int SnapshotChannel::GetCurrentVolume()
{
    const f32 lfElapsed  = mfElapsed;    // f0  = *(this+0x10)
    const f32 lfDuration = mfDuration;   // f13 = *(this+0x14)

    if (lfElapsed >= lfDuration)
    {
        return mi16TargetVolume;         // lhz r3, 0(r31); blr   (ramp done -> target)
    }

    const f32 lfRatio = lfElapsed / lfDuration;   // fdivs f1, f0, f13

    const int liBase   = mi16BaseVolume;          // extsh r11, lhz 4(r31)
    const int liTarget = mi16TargetVolume;        // extsh r10, lhz 0(r31)
    const int liCurve  = (liBase <= liTarget) ? 1 : 7;   // li r4,7 default; li r4,1 if base<=target

    const double lfBlend = SnapshotVolumeCurve(lfRatio, liCurve);   // sub_82B453C0(f1, r4)

    // delta = target - base (signed); fcfid->double, * blend, fctiwz->int, take low 16 bits.
    const int liDelta   = liTarget - liBase;                        // subf r10, r9, r10; extsw
    const int liProduct = static_cast<int>(static_cast<double>(liDelta) * lfBlend);  // fctiwz
    const int liResult  = liBase + static_cast<s16>(liProduct);     // add r3, (low16 product), r11

    return liResult;
}

// GetFinalVolume @ 0x82B472B8
//   if (mpCeiling == 0) return GetCurrentVolume();          // no ceiling: own volume
//   ceil = mpCeiling->GetFinalVolume();                     // recurse on the ceiling
//   v = GetCurrentVolume() + ceil;
//   v = (s16)v;                                             // extsh
//   if (v <= -10000) v = -10000;                            // clamp to DSound minimum
//   return (s16)v;
int SnapshotChannel::GetFinalVolume()
{
    SnapshotChannel* lpCeiling = mpCeiling;   // lwz r3, 0xC(r31)
    if (lpCeiling == nullptr)
    {
        return GetCurrentVolume();            // bl GetCurrentVolume (this) -> return
    }

    const int liCeiling = lpCeiling->GetFinalVolume();   // bl GetFinalVolume (on the ceiling)
    int liSum = static_cast<s16>(GetCurrentVolume() + liCeiling);   // add r11; extsh r11

    if (liSum <= -10000)                      // cmpwi r11, -0x2710; bgt ...
    {
        liSum = -10000;                       // li r11, -0x2710
    }
    return static_cast<s16>(liSum);           // extsh r3, r11
}

// GetCeilingVolume @ 0x82B47338
//   if (mpCeiling) return mpCeiling->GetFinalVolume();  else return 0;
int SnapshotChannel::GetCeilingVolume()
{
    SnapshotChannel* lpCeiling = mpCeiling;   // lwz r3, 0xC(r3)
    if (lpCeiling != nullptr)
    {
        return lpCeiling->GetFinalVolume();   // b GetFinalVolume (tail call, this = ceiling)
    }
    return 0;                                 // li r3, 0
}

} // namespace Nicotine
