// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 01.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::GetName             @ 0x822A6A70   (vtable slot 15)
//   BoostBurnout3::AreWeAllowedToBoost @ 0x822A6B90   (vtable slot 47)
//
// NOT here -- PARKED, see the wave report:
//   BoostBurnout3::ApplyUpdate         @ 0x822C1880   (vtable slot 1)
//   parked at scratchpad/waveP/parked/BoostBurnout3_wP_1.parked.cpp (workflow
//   repo). Its retail body re-reads the whole boostparamsasset tuning record
//   every update, and the committed Attrib::Gen::boostparamsasset exposes no
//   public accessor for the attribute data area (it inherits Attrib::Instance
//   PRIVATELY and re-exports nothing). Writing that body needs one line added
//   to a header this agent may not touch.
//
// Member layout / vtable slot order come from the wave-P keystone header
// BrnBoostStrategy.h; the BoostBurnout3-specific members (+0x130..+0x13C) are
// the DecFIGS DWARF order mfBoostChunkAmount / miBoostLevel / miOldBoostLevel /
// mfTimeBoosting, pinned on X360 by BoostBurnout3::Prepare @0x822C1680
// (stw 2 -> +0x134, stw 0 -> +0x138, stfs 0 -> +0x130 and +0x13C).
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// GetName @ 0x822A6A70 -- vtable slot 15.
//
//   lis   r11, aBurnout3Rules@ha
//   addi  r3,  r11, aBurnout3Rules@l   # "Burnout 3 rules"
//   blr
//
// Feb-2007 agrees verbatim (BrnBoostBurnout3.cpp:602 in the DecFIGS DWARF).
// ---------------------------------------------------------------------------
const char* BoostBurnout3::GetName() const
{
    return "Burnout 3 rules";
}

// ---------------------------------------------------------------------------
// AreWeAllowedToBoost @ 0x822A6B90 -- vtable slot 47. Consulted by
// BoostStrategy::SetBoostRequested @0x822A6090 and by the Update training-tip
// path @0x822F8238.
//
//   lbz   r11, 0xC5(r3)        # mbBoosting
//   cmplwi cr6, r11, 1
//   bne   cr6, loc_822A6BB8
//   lfs   f13, 0xA0(r3)        # mfBoostAmount
//   lfs   f0,  flt_82001CC0    # 0.0f
//   fcmpu cr6, f13, f0
//   ble   cr6, loc_822A6BCC    # -> return false
//   li    r3, 1 ; blr
// loc_822A6BB8:
//   lfs   f0,  0xA0(r3)        # mfBoostAmount
//   lfs   f13, 0x100(r3)       # mfMinBoostAllowedAmount
//   li    r3, 1
//   fcmpu cr6, f0, f13
//   bgtlr cr6                  # -> return true
// loc_822A6BCC:
//   li    r3, 0 ; blr
//
// A hysteresis gate: once boosting, any remaining boost keeps it legal; when
// not boosting, the bar must be above the per-car minimum before boost may be
// started. mfMinBoostAllowedAmount is seeded by BoostStrategy::
// SetCarStatBoostLevel @0x822D5304.
//
// NaN polarity (checked, not transliterated): the true-exits are the ORDERED
// predicates -- `ble` is taken when unordered (so a NaN mfBoostAmount returns
// false), and `bgtlr` is taken only when ordered-greater (so a NaN returns
// false there too). C++ `>` is false for NaN on both, so the plain `>` spelling
// below matches the branch senses exactly.
//
// No Feb-2007 counterpart: BoostBurnout3 gained this override after Feb-2007
// (the Feb-2007 BrnBoostBurnout3.cpp has no AreWeAllowedToBoost at all).
// ---------------------------------------------------------------------------
bool BoostBurnout3::AreWeAllowedToBoost()
{
    if (mbBoosting)
    {
        return mfBoostAmount > 0.0f;
    }

    return mfBoostAmount > mfMinBoostAllowedAmount;
}

}   // namespace BrnWorld
