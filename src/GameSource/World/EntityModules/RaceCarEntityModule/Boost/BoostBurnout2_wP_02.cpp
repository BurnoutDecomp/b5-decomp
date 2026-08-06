#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 02.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1).
//
// Functions in this partfile:
//   BoostBurnout2::GetName     @ 0x822A63E0
//   BoostBurnout2::OnCrash     @ 0x822A6370
//   BoostBurnout2::OnDriveThru @ 0x822C1648
//
// Offset -> member map used below (base layout from BrnBoostStrategy.h, the
// wave-P keystone; B2's own members start at +0x130 because sizeof(BoostStrategy)
// == 0x130 on BOTH console and host):
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0A4 BoostStrategy::mfMaxBoost
//   +0x0C5 BoostStrategy::mbBoosting
//   +0x130 BoostBurnout2::mfCrashDecrease      (DWARF BrnBoostBurnout2.h:132)
//   +0x134 BoostBurnout2::mfHiddenBoost        (DWARF BrnBoostBurnout2.h:134)
//   +0x138 BoostBurnout2::mbBoostInterrupted   (DWARF BrnBoostBurnout2.h:135)
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// GetName @ 0x822A63E0  (vtable slot 15)
//
//   0x822A63E0  lis   r11, aBurnout2Rules@ha
//   0x822A63E4  addi  r3, r11, aBurnout2Rules@l   # "Burnout 2 rules"
//   0x822A63E8  blr
//
// A single .rdata string address returned in r3; no `this` access at all.
// ---------------------------------------------------------------------------
const char*
BoostBurnout2::GetName() const
{
    return "Burnout 2 rules";
}

// ---------------------------------------------------------------------------
// OnCrash @ 0x822A6370  (vtable slot 7)
//
//   0x822A6370  lfs   f0,  0xA0(r3)        # mfBoostAmount
//   0x822A6378  lfs   f13, 0x130(r3)       # mfCrashDecrease
//   0x822A637C  fsubs f0, f0, f13
//   0x822A6380  stfs  f0,  0xA0(r3)        # mfBoostAmount -= mfCrashDecrease
//   0x822A6384  lfs   f13, flt_82001CC0    # 0.0f
//   0x822A6388  fcmpu cr6, f0, f13
//   0x822A638C  bge   cr6, loc_822A6394    # skip the clamp unless LT
//   0x822A6390  stfs  f13, 0xA0(r3)        # mfBoostAmount = 0.0f
//   0x822A6394  li    r11, 1
//   0x822A6398  li    r10, 0
//   0x822A639C  stb   r11, 0x138(r3)       # mbBoostInterrupted = true
//   0x822A63A0  stb   r10, 0xC5(r3)        # mbBoosting = false
//   0x822A63A4  blr
//
// The subtract+clamp pair IS BoostStrategy::RemoveBoost (DWARF
// BrnBoostStrategy.cpp:168, no standalone X360 symbol -- the compiler inlined
// it here and at every other call site); reversing that inline restores the
// original call, exactly as Feb-2007 wrote it.
//
// NaN polarity: `fcmpu` + `bge` is `bc 4,LT`, so the clamp store is reached
// only when the LT bit is set. Unordered (NaN) leaves LT clear and TAKES the
// branch, i.e. NaN does NOT get clamped -- which is precisely what C++
// `if (mfBoostAmount < 0.0f)` already means. No inversion needed here.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnCrash()
{
    RemoveBoost(mfCrashDecrease);

    mbBoostInterrupted = true;
    mbBoosting         = false;
}

// ---------------------------------------------------------------------------
// OnDriveThru @ 0x822C1648  (vtable slot 46)
//
//   0x822C1648  lbz    r11, 0xC5(r3)       # mbBoosting
//   0x822C164C  cmplwi cr6, r11, 0
//   0x822C1650  beq    cr6, loc_822C1660   # not boosting -> refill path
//   0x822C1654  lfs    f0, 0xA4(r3)        # mfMaxBoost
//   0x822C1658  stfs   f0, 0x134(r3)       # mfHiddenBoost = mfMaxBoost
//   0x822C165C  blr
//   0x822C1660  li     r4, 1
//   0x822C1664  b      BrnWorld__BoostStrategy__UpdateMaxBoost
//
// The tail-call carries r4 = 1, i.e. UpdateMaxBoost(lbFillToMax = true), which
// recomputes mfMaxBoost from miCombinedBoostLevel and then fills mfBoostAmount
// to it. UpdateMaxBoost is NON-virtual in the base (the branch target is the
// symbol itself, not a vtable load), so this is a direct call.
//
// While boosting the refill cannot be applied to the live bar, so it is parked
// in mfHiddenBoost -- the same stash BoostBurnout2 drains when a boost run
// ends. Note the store is an assignment (`stfs`), NOT the `+=` used by the
// OnSlam-style hidden-boost accumulations.
//
// No Feb-2007 counterpart exists: OnDriveThru was added after that drop, so
// this body is read entirely from the retail asm.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnDriveThru()
{
    if (mbBoosting)
    {
        mfHiddenBoost = mfMaxBoost;
    }
    else
    {
        UpdateMaxBoost(true);
    }
}

} // namespace BrnWorld
