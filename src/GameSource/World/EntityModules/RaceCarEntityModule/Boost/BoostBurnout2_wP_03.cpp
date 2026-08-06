// ============================================================================
// BrnWorld::BoostBurnout2 -- wave-P partfile 03.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies reconstructed in this partfile (X360 ARTIST addresses):
//   OnNearMiss             @0x822A62C0   (vtable slot 6)
//   OnEnterInfiniteBoost   @0x822C1668   (vtable slot 2)
//   OnEndCrashPlay         @0x822D5350   (vtable slot 14)
//
// Authority: the X360 asm listed inline below. The Feb-2007 source is a NAMING
// reference only here -- all three retail bodies CONTRADICT it (see the notes on
// each function); the asm wins.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnNearMiss -- vtable slot 6 (pure in the base). @0x822A62C0
//
//   0x822A62C0  cmplwi cr6, r4, 3        ; 4-case jump table, default = fall out
//   cases 0,1:  lbz r11,0xC5(r3)         ; mbBoosting
//               beq  -> 0x822A6310
//               lfs f0,0x134(r3) / lfs f13,0x10(r3) / fadds / stfs 0x134(r3)
//               blr
//     0x822A6310 lwz r11,0(r3) / lfs f1,0x10(r3) / lwz r11,0xC4(r11) / bctr
//                                        ; tail virtual call, vtable +0xC4 == slot 49
//                                        ; == BoostStrategy::AddBoost(mfNearMissBoostEarning)
//   cases 2,3:  same shape but with 0x8C(r3) == mfCrashEscapeBoostEarning
//   default:    blr                      ; nothing
//
// +0x10  == BoostStrategy::mfNearMissBoostEarning
// +0x8C  == BoostStrategy::mfCrashEscapeBoostEarning
// +0xC5  == BoostStrategy::mbBoosting
// +0x134 == BoostBurnout2::mfHiddenBoost
//
// DIVERGENCE FROM FEB-2007: the Feb-2007 body takes NO argument and always adds
// the single member `mfNearMissEarning`. Retail takes an ENearMissType and picks
// between two different tuning params -- the plain near-miss earning for the two
// NORMAL types (0/1) and the crash-escape earning for the two CRASH_ESCAPE types
// (2/3). The asm's 4-entry jump table pins the split at exactly 0,1 | 2,3.
//
// AddBoost is NOT overridden by BoostBurnout2, so the compiler emits a real
// vtable dispatch for the in-class call -- which is what the asm shows (bctr on
// slot 49). Writing the plain call reproduces it.
// ---------------------------------------------------------------------------
void BoostBurnout2::OnNearMiss(ENearMissType leNearMissType)
{
    switch (leNearMissType)
    {
    case E_NEAR_MISS_NORMAL_TRAFFIC:
    case E_NEAR_MISS_NORMAL_OTHER_RACE_CAR:
        // While boosting the earning is banked into the hidden pool instead of
        // going straight onto the bar.
        if (mbBoosting)
        {
            mfHiddenBoost += mfNearMissBoostEarning;
        }
        else
        {
            AddBoost(mfNearMissBoostEarning);
        }
        break;

    case E_NEAR_MISS_CRASH_ESCAPE_TRAFFIC:
    case E_NEAR_MISS_CRASH_ESCAPE_OTHER_RACE_CAR:
        if (mbBoosting)
        {
            mfHiddenBoost += mfCrashEscapeBoostEarning;
        }
        else
        {
            AddBoost(mfCrashEscapeBoostEarning);
        }
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// OnEnterInfiniteBoost -- vtable slot 2 (pure in the base). @0x822C1668
// Fired by BoostStrategy::SetInfiniteBoost on the false->true edge.
//
//   0x822C1668  lis  r11, flt_82014808@ha
//   0x822C166C  li   r4, 0                 ; lbFillToMax = false
//   0x822C1670  lfs  f0, flt_82014808@l(r11)   ; flt_82014808 == 100.0f
//   0x822C1674  stfs f0, 0xA0(r3)          ; mfBoostAmount = 100.0f
//   0x822C1678  b    BrnWorld__BoostStrategy__UpdateMaxBoost   ; DIRECT (non-virtual)
//
// 100.0f is the shared rodata literal at flt_82014808 (the same pool entry base
// Prepare @0x822A5C60 uses for mfMaxBoost and SetCarStatBoostLevel @0x822D52DC
// uses as a 0..100 clamp), not a named constant. It is deliberately "more than
// any possible bar": UpdateMaxBoost(false) immediately clamps mfBoostAmount into
// [0, mfMaxBoost], so the pair fills the bar to whatever the current max is.
//
// NOTE the direct branch to the base symbol: BoostStrategy::UpdateMaxBoost is
// NON-virtual. (BoostBurnout3's twin @0x822A6A80 instead dispatches through
// vtable +0xC8 == slot 50, which is B3's OWN `virtual UpdateMaxBoost(bool)`
// extension -- do not copy that shape here.)
//
// DIVERGENCE FROM FEB-2007: the Feb-2007 body is empty.
// ---------------------------------------------------------------------------
void BoostBurnout2::OnEnterInfiniteBoost()
{
    mfBoostAmount = 100.0f;
    BoostStrategy::UpdateMaxBoost(false);
}

// ---------------------------------------------------------------------------
// OnEndCrashPlay -- vtable slot 14. @0x822D5350
//
//   0x822D5350  lis  r11, flt_820147E8@ha
//   0x822D5354  lfs  f13, 0xA4(r3)         ; mfMaxBoost -- read BEFORE the update
//   0x822D5358  li   r10, 0
//   0x822D535C  li   r4, 0                 ; lbFillToMax = false
//   0x822D5360  lfs  f0, flt_820147E8@l(r11)   ; flt_820147E8 == 0.15f
//   0x822D5364  fmuls f0, f13, f0
//   0x822D5368  stw  r10, 0x104(r3)        ; miBoostLevel = 0
//   0x822D536C  stfs f0, 0x100(r3)         ; mfMinBoostAllowedAmount = mfMaxBoost * 0.15f
//   0x822D5370  b    BrnWorld__BoostStrategy__UpdateMaxBoost
//
// ORDER IS LOAD-BEARING: mfMaxBoost is sampled BEFORE UpdateMaxBoost runs, so the
// minimum-allowed amount is 15% of the PRE-update max. (BoostBurnout3's crash-play
// twin @0x822A6AF8 does the opposite -- it calls UpdateMaxBoost first and then
// multiplies the fresh mfMaxBoost. Do not normalise the two.)
//
// 0.15f is the shared rodata literal flt_820147E8 (BoostBurnout3 @0x822A6B48 loads
// the same entry for the same "15% floor" idiom); it is not a named constant in
// any recovered declaration, so it is written as a literal.
//
// DIVERGENCE FROM FEB-2007: the Feb-2007 body is empty.
// ---------------------------------------------------------------------------
void BoostBurnout2::OnEndCrashPlay()
{
    miBoostLevel = 0;
    mfMinBoostAllowedAmount = mfMaxBoost * 0.15f;
    BoostStrategy::UpdateMaxBoost(false);
}

} // namespace BrnWorld
