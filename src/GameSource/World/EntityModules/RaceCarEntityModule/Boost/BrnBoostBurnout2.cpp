// GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Created 2026-09-15 by tools/work/fold_partfiles.py --create-parent (b5-decomp issue #20).
// This family had NO parent TU: its bodies lived in 8 wave partfile(s), each
// with its own hand-written mount line in tools/build/build_game_exe.bat. They are folded
// here in MOUNT ORDER; every partfile's own header comment block is kept verbatim above
// its bodies (the address annotations are the evidence trail). No body was edited.
//
// Folded, in mount order:
//     BoostBurnout2_wP_01.cpp
//     BoostBurnout2_wP_02.cpp
//     BoostBurnout2_wP_03.cpp
//     BoostBurnout2_wP_04.cpp
//     BoostBurnout2_wP_05.cpp
//     BoostBurnout2_wP_06.cpp
//     BoostBurnout2_wP_11.cpp
//     BoostBurnout2_wP_16.cpp

// the union of the BoostBurnout2_w*.cpp partfiles' #include lines, first occurrence wins, mount order (2026-09-15)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"              // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "rw/core/stdc/stdc.h"                                                    // rw::core::stdc::ConvertI64ToA
#include "GameSource/GameState/BrnGameActions.h"                                  // BrnGameState::GameStateModuleIO::CompletedStuntAction
#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h" // BrnPhysics::EStuntActionComplete

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_01.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 01.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only and does NOT contain
// either of these two functions).
//
// Functions in this partfile:
//   BoostBurnout2::AreWeAllowedToBoost      @ 0x822A6408   (vtable slot 47)
//   BoostBurnout2::GetIsChainNotifyPending  @ 0x822A65A0   (vtable slot 18)
//
// A third function was assigned to this partfile -- BoostBurnout2::ApplyUpdate
// @0x822C1128 (slot 1). It is BLOCKED and parked out of tree, complete, at
// scratchpad/waveP/parked/BoostBurnout2_wP_1.parked.cpp: its tail re-reads all
// 34 Attrib::Gen::boostparamsasset tuning parameters, and the committed
// generated class (GameSource/AttribSys/Generated/classes/boostparamsasset.h)
// derives `private Instance` and exposes NOTHING but its constructor -- no
// attribute accessors and no `using Instance::GetLayoutPointer;`. There is no
// way to read the record without editing that header, which this pass may not
// do. See the parked file's banner for the exact declarations required.
//
// Offset -> member map used below (base layout from the wave-P keystone header
// BrnBoostStrategy.h; BoostBurnout2's own members start at +0x130 because
// sizeof(BoostStrategy) == 0x130 on BOTH console and host):
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0C5 BoostStrategy::mbBoosting
//   +0x0C9 BoostStrategy::mbInChainMode
//   +0x0CB BoostStrategy::mbChainNotifyPending
//   +0x100 BoostStrategy::mfMinBoostAllowedAmount
//   +0x13C BoostBurnout2::miChainSize          (DWARF BrnBoostBurnout2.h:137)
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// AreWeAllowedToBoost @ 0x822A6408  (vtable slot 47; pure in the base)
//
//   0x822A6408  lbz    r11, 0xC9(r3)          # mbInChainMode
//   0x822A640C  cmplwi cr6, r11, 0
//   0x822A6410  bne    cr6, loc_822A6454      # -> chain-mode arm
//   0x822A6414  lbz    r11, 0xC5(r3)          # mbBoosting
//   0x822A6418  cmplwi cr6, r11, 1
//   0x822A641C  bne    cr6, loc_822A643C      # -> not-boosting arm
//   0x822A6420  lis    r11, flt_82001CC0@ha
//   0x822A6424  lfs    f13, 0xA0(r3)          # mfBoostAmount
//   0x822A6428  lfs    f0,  flt_82001CC0@l    # 0.0f
//   0x822A642C  fcmpu  cr6, f13, f0
//   0x822A6430  ble    cr6, loc_822A646C      # -> return false
//   0x822A6434  li     r3, 1
//   0x822A6438  blr
//   0x822A643C  lfs    f0,  0xA0(r3)          # mfBoostAmount
//   0x822A6440  lfs    f13, 0x100(r3)         # mfMinBoostAllowedAmount
//   0x822A6444  fcmpu  cr6, f0, f13
//   0x822A6448  ble    cr6, loc_822A646C      # -> return false
//   0x822A644C  li     r3, 1
//   0x822A6450  blr
//   0x822A6454  lis    r11, flt_82001CC0@ha
//   0x822A6458  lfs    f13, 0xA0(r3)          # mfBoostAmount
//   0x822A645C  li     r3, 1
//   0x822A6460  lfs    f0,  flt_82001CC0@l    # 0.0f
//   0x822A6464  fcmpu  cr6, f13, f0
//   0x822A6468  bgtlr  cr6                    # return true iff ordered-greater
//   0x822A646C  li     r3, 0
//   0x822A6470  blr
//
// A three-arm chain on the boost-run state, each arm a single comparison:
//   * mid-chain (mbInChainMode)  -- any boost left at all keeps the run alive,
//     which is what lets a chain carry across the moment the bar empties;
//   * already boosting           -- same test, the run continues while > 0;
//   * neither                    -- a NEW run may only start once the bar is
//     above mfMinBoostAllowedAmount (the "one chunk" floor that
//     SetCarStatBoostLevel @0x822D5304 computes from mfMaxBoost).
//
// NaN polarity: every arm's true-exit is the ORDERED-greater case (`ble` falls
// to `li r3,0`, and `bgtlr` returns only on GT), so an unordered compare yields
// false on all three -- exactly what C++ `>` already means. No inversion needed.
//
// The `cmplwi r11, 1` against mbBoosting is the compiler's choice for a bool
// known to hold 0/1 (the same body uses `cmplwi ..., 0` for the identical test
// elsewhere -- see ApplyUpdate @0x822C12B8 vs @0x822C12F4); it is not a
// three-valued test.
// ---------------------------------------------------------------------------
bool
BoostBurnout2::AreWeAllowedToBoost()
{
    if (mbInChainMode)
    {
        return mfBoostAmount > 0.0f;
    }
    else if (mbBoosting)
    {
        return mfBoostAmount > 0.0f;
    }
    else
    {
        return mfBoostAmount > mfMinBoostAllowedAmount;
    }
}

// ---------------------------------------------------------------------------
// GetIsChainNotifyPending @ 0x822A65A0  (vtable slot 18; the base body
// @0x822A5DF0 is overridden here for the chain-capable Burnout 2 rules)
//
//   0x822A65B4  mr     r30, r4                # lpOutNumChained
//   0x822A65B8  mr     r31, r3                # this
//   0x822A65BC  cmplwi cr6, r30, 0
//   0x822A65C0  bne    cr6, loc_822A65E4
//   0x822A65C4  bl     CgsDev__Assert__BeginAssert
//   0x822A65CC  lis    r11, aDP4B5MainBurno_530@ha
//   0x822A65D0  li     r5, 0x32B              # line 811
//   0x822A65D4  addi   r4, r11, ...@l         # "d:\p4\b5_main\...\BrnBoostBurnout2.cpp"
//   0x822A65D8  lis    r11, aLpoutnumchaine@ha
//   0x822A65DC  addi   r3, r11, ...@l         # "lpOutNumChained"
//   0x822A65DC  bl     CgsDev__Assert__FireAssert
//   0x822A65E0  bl     CgsDev__Assert__EndAssert
//   0x822A65E4  lwz    r11, 0x13C(r31)        # miChainSize
//   0x822A65E8  li     r10, 0
//   0x822A65EC  cmpwi  cr6, r11, 0            # SIGNED compare
//   0x822A65F0  bgt    cr6, loc_822A65FC
//   0x822A65F4  stw    r10, 0(r30)            # *lpOutNumChained = 0
//   0x822A65F8  b      loc_822A6600
//   0x822A65FC  stw    r11, 0(r30)            # *lpOutNumChained = miChainSize
//   0x822A6600  lbz    r3,  0xCB(r31)         # return mbChainNotifyPending
//   0x822A6604  stb    r10, 0xCB(r31)         # ...and clear it (read-and-clear)
//   0x822A661C  blr
//
// Two details the asm pins that a Feb-2007 read would get wrong (this function
// does not exist in that drop at all):
//   * the assert does NOT early-out. The store through lpOutNumChained is
//     unconditional and is reached whether or not the pointer was null --
//     CGS_ASSERT is a report-and-continue macro on this build, matching the
//     bare Begin/Fire/End triple emitted here.
//   * miChainSize is compared SIGNED (`cmpwi`) against 0 and only a strictly
//     positive value is published; the out-parameter is uint32_t (DWARF
//     BrnBoostBurnout2.h `virtual bool GetIsChainNotifyPending(uint32_t *)`),
//     so the guard is what keeps a negative chain size from being published as
//     a huge unsigned count.
// The read-and-clear of mbChainNotifyPending is the same latch idiom as the
// base IsATrafficCheckPending @0x822A5E98 / HasJustLostBoostChunk @0x822A5EC0.
// ---------------------------------------------------------------------------
bool
BoostBurnout2::GetIsChainNotifyPending(u32* lpOutNumChained)
{
    // BrnBoostBurnout2.cpp:811 on the console (the assert's line immediate is
    // 0x32B); reported, then execution continues into the store below.
    CGS_ASSERT(lpOutNumChained != 0, "lpOutNumChained");

    if (miChainSize > 0)
    {
        *lpOutNumChained = static_cast<u32>(miChainSize);
    }
    else
    {
        *lpOutNumChained = 0;
    }

    const bool lbChainNotifyPending = mbChainNotifyPending;
    mbChainNotifyPending = false;

    return lbChainNotifyPending;
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_02.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

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

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_03.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
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

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_04.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout2 -- wave-P partfile 04.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies reconstructed in this partfile (X360 ARTIST addresses):
//   OnPlayerAttacksRival  @0x822A6E68   (vtable slot 5)
//   OnPropHit             @0x822A63A8   (vtable slot 16)
//   OnStuntCompletion     @0x822A6210   (vtable slot 10)
//
// Authority: the X360 ARTIST asm quoted inline below (rung 1). ALL THREE bodies
// CONTRADICT the Feb-2007 drop -- OnPlayerAttacksRival and OnPropHit do not
// exist there at all, and Feb-2007's OnStuntCompletion takes no argument and
// just forwards to OnTakedown(). The asm wins; see the per-function notes.
//
// Offset -> member map used below (base layout from the wave-P keystone header
// BrnBoostStrategy.h; B2's own members start at +0x130 because
// sizeof(BoostStrategy) == 0x130 on BOTH console and host):
//   +0x000 vptr; vtable +0xC4 == slot 49 == BoostStrategy::AddBoost(f32)
//   +0x02C BoostStrategy::mfShuntEarning
//   +0x030 BoostStrategy::mfSlamEarning
//   +0x038 BoostStrategy::mfTradingPaintEarning
//   +0x03C BoostStrategy::mfGrindingEarning
//   +0x040 BoostStrategy::mfRubbingEarning
//   +0x080 BoostStrategy::mfStuntJumpEarning
//   +0x084 BoostStrategy::mfStuntSmashEarning
//   +0x088 BoostStrategy::mfStuntBillBoardEarning
//   +0x0C5 BoostStrategy::mbBoosting
//   +0x134 BoostBurnout2::mfHiddenBoost          (DWARF BrnBoostBurnout2.h:134)
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnPlayerAttacksRival -- vtable slot 5 (pure in the base). @0x822A6E68
//
//   0x822A6E68  addi   r11, r4, -1
//   0x822A6E6C  cmplwi cr6, r11, 7          ; UNSIGNED -> leImpactType 0 (and
//   0x822A6E70  bgtlr  cr6                  ;   anything > 8) returns here
//   0x822A6E74  lis    r12, jpt_822A6E88@ha
//   0x822A6E78  addi   r12, r12, jpt_822A6E88@l
//   0x822A6E7C  slwi   r0, r11, 2
//   0x822A6E80  lwzx   r0, r12, r0
//   0x822A6E84  mtctr  r0
//   0x822A6E88  bctr                        ; 8-entry table, index = type - 1
//   0x822A6E8C  .long  loc_822A6ED4         ; [0] -> the +0x38 arm
//   0x822A6EAC  lwz r11,0(r3) / lfs f1,0x30(r3) / lwz r11,0xC4(r11) / bctr
//                                           ; table indices 2,4  (types 3,5)
//   0x822A6EC0  lwz r11,0(r3) / lfs f1,0x2C(r3) / lwz r11,0xC4(r11) / bctr
//                                           ; table indices 3,5  (types 4,6)
//   0x822A6ED4  lwz r11,0(r3) / lfs f1,0x38(r3) / lwz r11,0xC4(r11) / bctr
//                                           ; table index   0    (type  1)
//   0x822A6EE8  lwz r11,0(r3) / lfs f1,0x3C(r3) / lwz r11,0xC4(r11) / bctr
//                                           ; table index   6    (type  7)
//   0x822A6EFC  lwz r11,0(r3) / lfs f1,0x40(r3) / lwz r11,0xC4(r11) / bctr
//                                           ; table index   7    (type  8)
//   0x822A6F10  blr                         ; table index   1    (type  2)
//
// The six arm labels' jump-table indices partition {0..7} exactly, and the
// resulting (impact type -> tuning param) map lines up name-for-name with the
// committed BrnPhysics::Vehicle::EImpactType enumerators -- an independent
// confirmation of both the base offset map and the enum values:
//
//   E_IMPACT_TRADING_PAINT (1) -> +0x38 mfTradingPaintEarning
//   E_IMPACT_NUDGE         (2) -> nothing at all (the bare `blr` arm)
//   E_IMPACT_SLAM          (3) -> +0x30 mfSlamEarning
//   E_IMPACT_SHUNT         (4) -> +0x2C mfShuntEarning
//   E_IMPACT_BOOST_SLAM    (5) -> +0x30 mfSlamEarning     (shares SLAM's arm)
//   E_IMPACT_BOOST_SHUNT   (6) -> +0x2C mfShuntEarning    (shares SHUNT's arm)
//   E_IMPACT_GRINDING      (7) -> +0x3C mfGrindingEarning
//   E_IMPACT_RUBBING       (8) -> +0x40 mfRubbingEarning
//   E_IMPACT_NONE          (0) -> falls out of the unsigned range check
//
// mfNudgeEarning (+0x34) is deliberately NOT read by this function: nudges earn
// the player nothing when attacking a rival. It is the only earning param in
// the +0x2C..+0x40 run that this switch skips.
//
// EVERY arm is a TAIL virtual dispatch through vtable +0xC4 == slot 49 ==
// BoostStrategy::AddBoost(f32). BoostBurnout2 does NOT override AddBoost, so
// the compiler emits a real vtable call for the in-class call and the plain
// `AddBoost(...)` below reproduces the asm exactly. Note there is NO mbBoosting
// / mfHiddenBoost banking here -- unlike OnPropHit / OnStuntCompletion /
// OnTakedown, this reward always goes straight onto the bar.
//
// DIVERGENCE FROM FEB-2007: BoostBurnout2::OnPlayerAttacksRival does not exist
// in the Feb-2007 drop (no declaration, no body, anywhere under
// .../RaceCarEntityModule/Boost/). This body is read entirely from retail asm.
// ---------------------------------------------------------------------------
void BoostBurnout2::OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType)
{
    switch (leImpactType)
    {
    case BrnPhysics::Vehicle::E_IMPACT_TRADING_PAINT:
        AddBoost(mfTradingPaintEarning);
        break;

    case BrnPhysics::Vehicle::E_IMPACT_SLAM:
    case BrnPhysics::Vehicle::E_IMPACT_BOOST_SLAM:
        AddBoost(mfSlamEarning);
        break;

    case BrnPhysics::Vehicle::E_IMPACT_SHUNT:
    case BrnPhysics::Vehicle::E_IMPACT_BOOST_SHUNT:
        AddBoost(mfShuntEarning);
        break;

    case BrnPhysics::Vehicle::E_IMPACT_GRINDING:
        AddBoost(mfGrindingEarning);
        break;

    case BrnPhysics::Vehicle::E_IMPACT_RUBBING:
        AddBoost(mfRubbingEarning);
        break;

    // E_IMPACT_NONE and E_IMPACT_NUDGE earn nothing.
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// OnPropHit -- vtable slot 16 (NON-pure in the base; B2 overrides it).
// @0x822A63A8
//
//   0x822A63A8  lbz    r11, 0xC5(r3)        ; mbBoosting
//   0x822A63AC  cmplwi cr6, r11, 0
//   0x822A63B0  beq    cr6, loc_822A63C8
//   0x822A63B4  lfs    f0,  0x134(r3)       ; mfHiddenBoost
//   0x822A63B8  lfs    f13, 0x84(r3)        ; mfStuntSmashEarning
//   0x822A63BC  fadds  f0, f13, f0
//   0x822A63C0  stfs   f0,  0x134(r3)       ; mfHiddenBoost += mfStuntSmashEarning
//   0x822A63C4  blr
//   0x822A63C8  lwz    r11, 0(r3)
//   0x822A63CC  lfs    f1,  0x84(r3)
//   0x822A63D0  lwz    r11, 0xC4(r11)       ; slot 49 == AddBoost
//   0x822A63D4  mtctr  r11
//   0x822A63D8  bctr                        ; AddBoost(mfStuntSmashEarning)
//
// The "bank it while boosting, otherwise put it on the bar" idiom is the same
// one Feb-2007 wrote out longhand in OnNearMiss; retail reuses it verbatim
// here. The param read is +0x84 == mfStuntSmashEarning, i.e. hitting a
// smashable prop is what pays the SMASH stunt earning -- which is exactly why
// OnStuntCompletion below has no E_STUNT_ELEMENT_TYPE_SMASH arm.
//
// DIVERGENCE FROM FEB-2007: BoostBurnout2::OnPropHit does not exist in the
// Feb-2007 drop. This body is read entirely from retail asm.
// ---------------------------------------------------------------------------
void BoostBurnout2::OnPropHit()
{
    // While boosting the earning is banked into the hidden pool instead of
    // going straight onto the bar.
    if (mbBoosting)
    {
        mfHiddenBoost += mfStuntSmashEarning;
    }
    else
    {
        AddBoost(mfStuntSmashEarning);
    }
}

// ---------------------------------------------------------------------------
// OnStuntCompletion -- vtable slot 10 (pure in the base). @0x822A6210
//
//   0x822A6210  cmpwi  cr6, r4, 0
//   0x822A6214  beq    cr6, loc_822A6254            ; -> the JUMP arm
//   0x822A6218  cmpwi  cr6, r4, 2
//   0x822A621C  bne    cr6, locret_822A623C         ; anything else: nothing
//   0x822A6220  lbz    r11, 0xC5(r3)                ; mbBoosting
//   0x822A6224  cmplwi cr6, r11, 0
//   0x822A6228  beq    cr6, loc_822A6240
//   0x822A622C  lfs    f0,  0x134(r3)               ; mfHiddenBoost
//   0x822A6230  lfs    f13, 0x88(r3)                ; mfStuntBillBoardEarning
//   0x822A6234  fadds  f0, f13, f0
//   0x822A6238  stfs   f0,  0x134(r3)
//   0x822A623C  blr
//   0x822A6240  lwz    r11, 0(r3)
//   0x822A6244  lfs    f1,  0x88(r3)
//   0x822A6248  lwz    r11, 0xC4(r11)               ; slot 49 == AddBoost
//   0x822A624C  mtctr  r11
//   0x822A6250  bctr                                ; AddBoost(mfStuntBillBoardEarning)
//   0x822A6254  lbz    r11, 0xC5(r3)                ; mbBoosting
//   0x822A6258  cmplwi cr6, r11, 0
//   0x822A625C  beq    cr6, loc_822A6274
//   0x822A6260  lfs    f0,  0x134(r3)               ; mfHiddenBoost
//   0x822A6264  lfs    f13, 0x80(r3)                ; mfStuntJumpEarning
//   0x822A6268  fadds  f0, f13, f0
//   0x822A626C  stfs   f0,  0x134(r3)
//   0x822A6270  blr
//   0x822A6274  lwz    r11, 0(r3)
//   0x822A6278  lfs    f1,  0x80(r3)
//   0x822A627C  lwz    r11, 0xC4(r11)
//   0x822A6280  mtctr  r11
//   0x822A6284  bctr                                ; AddBoost(mfStuntJumpEarning)
//
// Two arms only, keyed on BrnGameState::StuntElementType:
//   E_STUNT_ELEMENT_TYPE_JUMP      (0) -> +0x80 mfStuntJumpEarning
//   E_STUNT_ELEMENT_TYPE_SMASH     (1) -> nothing (paid by OnPropHit instead)
//   E_STUNT_ELEMENT_TYPE_BILLBOARD (2) -> +0x88 mfStuntBillBoardEarning
// The comparisons are SIGNED (`cmpwi`), matching the plain (no fixed underlying
// type) `enum StuntElementType`.
//
// Both arms use the same bank-while-boosting idiom as OnPropHit.
//
// DIVERGENCE FROM FEB-2007: Feb-2007's `BoostBurnout2::OnStuntCompletion()`
// takes NO argument and its whole body is `OnTakedown();` (i.e. it slammed the
// bar to full: `mfBoostAmount = mfMaxBoost;`). Retail took a StuntElementType,
// split jump vs billboard onto two separate tuning params, moved the smash case
// out to OnPropHit, and routes through AddBoost/mfHiddenBoost instead of
// OnTakedown. Nothing of the Feb-2007 body survives.
// ---------------------------------------------------------------------------
void BoostBurnout2::OnStuntCompletion(BrnGameState::StuntElementType leElementType)
{
    switch (leElementType)
    {
    case BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP:
        if (mbBoosting)
        {
            mfHiddenBoost += mfStuntJumpEarning;
        }
        else
        {
            AddBoost(mfStuntJumpEarning);
        }
        break;

    case BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD:
        if (mbBoosting)
        {
            mfHiddenBoost += mfStuntBillBoardEarning;
        }
        else
        {
            AddBoost(mfStuntBillBoardEarning);
        }
        break;

    // E_STUNT_ELEMENT_TYPE_SMASH earns nothing here -- the smash reward is paid
    // by OnPropHit @0x822A63A8, which reads the same mfStuntSmashEarning param.
    default:
        break;
    }
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_05.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout2 -- wave-P partfile 05.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies reconstructed in this partfile (X360 ARTIST addresses):
//   OnTakedown      @0x822A6288   (vtable slot 3)
//   OnTrafficCheck  @0x822A6358   (vtable slot 12)
//   OnWrecked       @0x822C15D8   (vtable slot 8)
//
// Authority: the X360 ARTIST asm quoted inline below (rung 1). Feb-2007 is used
// for idiom/naming only and CONTRADICTS retail on OnTakedown; it has no
// OnWrecked at all (the whole wreck path post-dates that drop). See the
// per-function DIVERGENCE notes.
//
// Offset -> member map used below. Base layout from the wave-P keystone header
// BrnBoostStrategy.h; BoostBurnout2's own members start at +0x130 because
// sizeof(BoostStrategy) == 0x130 on BOTH console and host:
//   +0x028 BoostStrategy::mfTakedownEarning     (attrib rec+0x08 TakedownEarning)
//   +0x048 BoostStrategy::mfTrafficCheck        (attrib rec+0x00 TrafficCheck)
//   +0x094 BoostStrategy::mfOnWrecked           (attrib rec+0x30 OnWrecked)
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0A4 BoostStrategy::mfMaxBoost
//   +0x0C5 BoostStrategy::mbBoosting
//   +0x134 BoostBurnout2::mfHiddenBoost         (DWARF BrnBoostBurnout2.h:134)
//   +0x138 BoostBurnout2::mbBoostInterrupted    (DWARF BrnBoostBurnout2.h:135)
//
// Shared rodata literal used by all three: flt_82001CC0 == 0.0f, pinned by
// RemoveAllBoostAndChunks @0x822A63F0 (Feb-2007 body: mfHiddenBoost = 0.0f;
// mfBoostAmount = 0.0f; the asm stores flt_82001CC0 to both +0x134 and +0xA0).
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnTakedown -- vtable slot 3 (pure in the base). @0x822A6288
//
//   0x822A6288  lbz    r11, 0xC5(r3)      ; mbBoosting
//   0x822A628C  cmplwi cr6, r11, 0
//   0x822A6290  beq    cr6, loc_822A62A8
//   0x822A6294  lfs    f0,  0x134(r3)     ; mfHiddenBoost
//   0x822A6298  lfs    f13, 0x28(r3)      ; mfTakedownEarning
//   0x822A629C  fadds  f0, f13, f0
//   0x822A62A0  stfs   f0,  0x134(r3)     ; mfHiddenBoost += mfTakedownEarning
//   0x822A62A4  blr
//   0x822A62A8  lwz    r11, 0(r3)         ; vtable
//   0x822A62AC  lfs    f1,  0x28(r3)      ; mfTakedownEarning -> f1 (PPC float arg)
//   0x822A62B0  lwz    r11, 0xC4(r11)     ; +0xC4 == slot 49 == AddBoost
//   0x822A62B4  mtctr  r11
//   0x822A62B8  bctr                      ; tail virtual call
//
// The two-way "bank it while boosting, otherwise put it on the bar" shape is
// BoostBurnout2's house idiom -- Feb-2007 writes exactly this in OnNearMiss and
// OnSlam, so the member name mfHiddenBoost and the `+=` are style-confirmed.
// BoostBurnout2 does not override AddBoost, so the in-class call still goes
// through the vtable, which is what the `bctr` on slot 49 shows.
//
// DIVERGENCE FROM FEB-2007: the Feb-2007 body is a one-liner,
// `mfBoostAmount = mfMaxBoost;` (BrnBoostBurnout2.cpp:172) -- an instant full
// bar. Retail replaced it with a tuned earning that respects AddBoost's
// speed-scaling/earning gates. The asm wins: neither mfBoostAmount (+0xA0) nor
// mfMaxBoost (+0xA4) is touched anywhere in this body.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnTakedown()
{
    if (mbBoosting)
    {
        // Cannot grow the live bar mid-boost; bank it in the hidden pool.
        mfHiddenBoost += mfTakedownEarning;
    }
    else
    {
        AddBoost(mfTakedownEarning);
    }
}

// ---------------------------------------------------------------------------
// OnTrafficCheck -- vtable slot 12 (pure in the base). @0x822A6358
//
//   0x822A6358  lwz   r11, 0(r3)          ; vtable
//   0x822A635C  lfs   f1,  0x48(r3)       ; mfTrafficCheck -> f1
//   0x822A6360  lwz   r11, 0xC4(r11)      ; +0xC4 == slot 49 == AddBoost
//   0x822A6364  mtctr r11
//   0x822A6368  bctr                      ; tail virtual call
//
// Five instructions, no `this` state touched: a bare AddBoost. NOTE the
// asymmetry with OnTakedown above -- there is NO mbBoosting / mfHiddenBoost
// branch here, and mbJustTrafficChecked (+0xC2) is NOT set (the flag is raised
// by the traffic-check detector, not by this reward hook). Do not "normalise"
// this body against OnTakedown's shape.
//
// FEB-2007 agrees on shape (BrnBoostBurnout2.cpp:219,
// `AddBoost( mfOnTrafficCheckBoostReward );`); only the member NAME changed --
// the retail/DecFIGS base member for attrib record offset rec+0x00
// ("TrafficCheck") is BoostStrategy::mfTrafficCheck at +0x48.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnTrafficCheck()
{
    AddBoost(mfTrafficCheck);
}

// ---------------------------------------------------------------------------
// OnWrecked -- vtable slot 8 (pure in the base). @0x822C15D8
// Fired by BoostStrategy::SetWrecking on the false->true edge (X360: inlined
// into BoostManager::SetWrecking @0x822B8E40).
//
//   0x822C15D8  lbz    r11, 0xC5(r3)      ; mbBoosting
//   0x822C15DC  cmplwi cr6, r11, 0
//   0x822C15E0  lis    r11, flt_82001CC0@ha
//   0x822C15E4  lfs    f0,  flt_82001CC0@l(r11)  ; f0 = 0.0f
//   0x822C15E8  beq    cr6, loc_822C1608
//   0x822C15EC  lfs    f13, 0xA4(r3)      ; mfMaxBoost
//   0x822C15F0  li     r11, 0
//   0x822C15F4  fsubs  f12, f13, f0       ; mfMaxBoost - 0.0f
//   0x822C15F8  stfs   f0,  0x134(r3)     ; mfHiddenBoost      = 0.0f
//   0x822C15FC  stb    r11, 0x138(r3)     ; mbBoostInterrupted = false
//   0x822C1600  fsel   f13, f12, f0, f13  ; (mfMaxBoost-0 >= 0) ? 0.0f : mfMaxBoost
//   0x822C1604  stfs   f13, 0xA0(r3)      ; mfBoostAmount = that
//   loc_822C1608:
//   0x822C1608  lfs    f13, 0xA0(r3)      ; mfBoostAmount
//   0x822C160C  lfs    f12, 0x94(r3)      ; mfOnWrecked
//   0x822C1610  fsubs  f13, f13, f12
//   0x822C1614  stfs   f13, 0xA0(r3)      ; mfBoostAmount -= mfOnWrecked
//   0x822C1618  fcmpu  cr6, f13, f0
//   0x822C161C  bge    cr6, loc_822C1624
//   0x822C1620  stfs   f0,  0xA0(r3)      ; ...clamped at zero
//   loc_822C1624:
//   0x822C1624  lfs    f13, 0xA0(r3)
//   0x822C1628  fneg   f11, f13
//   0x822C162C  lfs    f12, 0xA4(r3)      ; mfMaxBoost
//   0x822C1630  fsel   f0,  f11, f0, f13  ; (-v >= 0) ? 0.0f : v      == Max(0, v)
//   0x822C1634  fsubs  f13, f12, f0
//   0x822C1638  fsel   f0,  f13, f0, f12  ; (max - t >= 0) ? t : max  == Min(t, max)
//   0x822C163C  stfs   f0,  0xA0(r3)
//   0x822C1640  blr
//
// THE PARAMETER IS UNUSED. DWARF declares `virtual void OnWrecked(bool)` and
// DecFIGS names it lbIsInOnlineGameMode, but r4 is never read anywhere in this
// body -- BoostBurnout2 wrecks identically either way. (Register check: the
// only GPR sourced is r3 = this; the two float operands come from `this`, not
// from f1, so there is no hidden PPC float argument being missed here.)
//
// THE SUBTRACT+CLAMP-AT-ZERO PAIR (0x822C1608..0x822C1620) IS
// BoostStrategy::RemoveBoost (DecFIGS BrnBoostStrategy.cpp:168) inlined -- it
// is byte-for-byte the same shape the compiler emitted in OnCrash @0x822A6374,
// where the same helper is inlined. Reversing the inline restores the call.
// NaN polarity there needs no inversion: `fcmpu`+`bge` is `bc 4,LT`, so the
// clamp store is reached only when LT is set; an unordered compare leaves LT
// clear and TAKES the branch -- exactly what C++ `if (x < 0.0f)` means.
//
// THE TAIL (0x822C1624..0x822C163C) IS THE SHARED [0, mfMaxBoost] CLAMP: it is
// instruction-for-instruction the same fsel pair the compiler emits at the tail
// of BoostStrategy::UpdateMaxBoost @0x822C0EF4..0x822C0F14 and
// BoostStrategy::AddBoost @0x822C0E88..0x822C0EA4. The f32 Min/Max helper it
// was inlined from is not present in any recovered header, so it is written out
// here. NaN polarity IS load-bearing on the upper half: fsel yields mfMaxBoost
// when the compare is unordered, so the ordered predicate must be negated -- a
// plain `if (x > mfMaxBoost)` would leave a NaN unclamped and diverge.
//
// The in-branch store at 0x822C1604 is that SAME clamp applied to a literal
// 0.0f: the compiler constant-folded the Max(0, 0) half away and kept only the
// Min against mfMaxBoost (hence the otherwise-pointless `fsubs f12, f13, f0`
// against zero). Written below in the fsel's own sense so the unordered case
// still yields mfMaxBoost.
//
// FEB-2007 HAS NO COUNTERPART: neither OnWrecked nor SetWrecking exists
// anywhere in the Feb-2007 Boost tree, so this body is read entirely from the
// retail asm.
// ---------------------------------------------------------------------------
void
BoostBurnout2::OnWrecked(bool /* lbIsInOnlineGameMode -- unused; r4 never read */)
{
    if (mbBoosting)
    {
        // A wreck kills the run outright: the banked pool is forfeited (+0x134
        // = 0.0f) and the bar is emptied.
        //
        // CLEARING mbBoostInterrupted RE-ARMS the chain payout, it does not
        // suppress it -- CORRECTED 2026-08-06, this comment used to claim the
        // opposite. Re-derived from the asm: ApplyUpdate @0x822C1410 reads
        // +0x138 and `bne` @0x822C141C SKIPS the
        // `AddBoost(mfHiddenBoost + mfBoostChainBonus)` + mbChainNotifyPending +
        // ++miChainSize payout whenever the flag is NON-zero, and OnCrash
        // @0x822A639C stores 1 into it precisely to suppress that path. Storing
        // 0 here therefore leaves the payout path OPEN; what makes the wreck
        // cost nothing is mfHiddenBoost being zeroed on the line above, not the
        // flag. The code below is correct as written -- do not "fix" it.
        mfHiddenBoost      = 0.0f;
        mbBoostInterrupted = false;
        mfBoostAmount      = (mfMaxBoost >= 0.0f) ? 0.0f : mfMaxBoost;
    }

    RemoveBoost(mfOnWrecked);

    // Shared [0, mfMaxBoost] clamp (see the note above).
    if (mfBoostAmount < 0.0f)
    {
        mfBoostAmount = 0.0f;
    }
    if (!(mfBoostAmount <= mfMaxBoost))
    {
        mfBoostAmount = mfMaxBoost;
    }
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_06.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 06.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX; the
// ARTIST asm is rung 1 and arbitrates every branch/constant/store below):
//   BoostBurnout2::RemoveAllBoostAndChunks @ 0x822A63F0   (vtable slot 41)
//
// NOT here -- PARKED complete-but-unlandable, see the wave report and
// scratchpad/waveP/parked/BoostBurnout2_wP_6.parked.cpp (workflow repo):
//   BoostBurnout2::Prepare          @ 0x822C0F38  (vtable slot 0)
//       needs 34 attribute accessors on Attrib::Gen::boostparamsasset -- the
//       committed generated class derives Attrib::Instance PRIVATELY and
//       re-exports nothing, so its 0x88-byte data area is unreachable from
//       this TU. (Same blocker the BoostBurnout3 ApplyUpdate park hit.)
//   BoostBurnout2::UpdateStuntBoost @ 0x822A6478  (vtable slot 48)
//       needs the complete type BrnGameState::GameStateModuleIO::
//       CompletedStuntAction; BrnBoostStrategy.h only forward-declares it and
//       no header in b5-decomp/src defines it.
//
// Offset -> member map used below (base layout from the wave-P keystone header
// BrnBoostStrategy.h; BoostBurnout2's own members start at +0x130 because
// sizeof(BoostStrategy) == 0x130 on BOTH console and host):
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x134 BoostBurnout2::mfHiddenBoost   (DWARF BrnBoostBurnout2.h:134)
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// RemoveAllBoostAndChunks @ 0x822A63F0 -- vtable slot 41 (pure in the base;
// all three concrete strategies override it).
//
//   0x822A63F0  lis   r11, flt_82001CC0@ha
//   0x822A63F4  lfs   f0,  flt_82001CC0@l(r11)   # 0.0f
//   0x822A63F8  stfs  f0,  0x134(r3)             # mfHiddenBoost = 0.0f
//   0x822A63FC  stfs  f0,  0xA0(r3)              # mfBoostAmount = 0.0f
//   0x822A6400  blr
//
// Five instructions, no branches, no `this` reads: both stores are the single
// shared 0.0f constant flt_82001CC0. The Hex-Rays `int __fastcall ...(int
// result)` / `return result` is the usual PPC artifact of r3 (the incoming
// `this`) surviving to the blr -- the DWARF declares `virtual void
// RemoveAllBoostAndChunks()` (BrnBoostBurnout2.h:102 / cpp:709) and nothing in
// the image consumes a return value from slot 41, so the return type is void.
//
// The live bar AND the parked stash both go to zero: mfHiddenBoost is the
// accumulator BoostBurnout2 fills while a boost run is in flight (OnDriveThru
// @0x822C1648, the OnSlam-family adds) and drains when the run ends, so
// clearing mfBoostAmount alone would let the stash refill the bar on the next
// drain. Retail and Feb-2007 agree here verbatim, store order included
// (Feb-2007 BrnBoostBurnout2.cpp:393-397).
// ---------------------------------------------------------------------------
void
BoostBurnout2::RemoveAllBoostAndChunks()
{
    mfHiddenBoost = 0.0f;
    mfBoostAmount = 0.0f;
}

}   // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_11.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 11.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Body in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX; the ARTIST
// asm is rung 1 and arbitrates every branch, constant and store below):
//   BoostBurnout2::ApplyUpdate @ 0x822C1128   (base vtable slot 1, pure in base)
//
// Parked earlier in wave P on one header blocker that is now gone:
//   Attrib::Gen::boostparamsasset exposes the 34 named tuning-parameter
//   accessors (GameSource/AttribSys/Generated/classes/boostparamsasset.h), so
//   the per-frame re-read at the tail goes through those accessors. This TU does
//   NOT re-export or poke Attrib::Instance::GetLayoutPointer.
//
// Offset -> member map used below. BoostBurnout2's own members start at +0x130
// because sizeof(BoostStrategy) == 0x130 on BOTH console and host; the 34 tuning
// params and the shared flags live in the base (BrnBoostStrategy.h).
//   +0x014 BoostStrategy::mfDriftEarning          +0x0A4 BoostStrategy::mfMaxBoost
//   +0x018 BoostStrategy::mfAirEarning            +0x0B4 BoostStrategy::mfSpeed
//   +0x044 BoostStrategy::mfTailgatingEarning     +0x0C3 BoostStrategy::mbIsBoostFull
//   +0x070 BoostStrategy::mfBurnRateBoost         +0x0C5 BoostStrategy::mbBoosting
//   +0x078 BoostStrategy::mfBoostOnComing         +0x0C7 BoostStrategy::mbBoostRequested
//   +0x090 BoostStrategy::mfBoostChainBonus       +0x0C9 BoostStrategy::mbInChainMode
//   +0x098 BoostStrategy::mfTimeSpentCheating     +0x0CB BoostStrategy::mbChainNotifyPending
//   +0x0A0 BoostStrategy::mfBoostAmount           +0x0F0 BoostStrategy::mfCurrentCarBoostLossLevel
//   +0x120 BoostStrategy::mfTotalDistanceTraveled
//   +0x134 BoostBurnout2::mfHiddenBoost           +0x144 BoostBurnout2::mfContinuousBoostingTimeAdd
//   +0x138 BoostBurnout2::mbBoostInterrupted      +0x148 BoostBurnout2::mfTimeBoosting
//   +0x13C BoostBurnout2::miChainSize
//   +0x140 BoostBurnout2::mfTimeBasedBoostingBonus
//
// WHAT THE ASM SAYS THAT FEB-2007 DOES NOT
//  * Retail ApplyUpdate takes ONLY (this, f1 = lfTimeStep). The Feb-2007
//    `CarOffenceManager* lpCarOffenceManager` second parameter is gone, and so is
//    the `lpCarOffenceManager->OnBoostChain()` call: the chain notification became
//    the mbChainNotifyPending / miChainSize latch that GetIsChainNotifyPending
//    @0x822A65A0 drains.
//  * Retail RELOADS the whole boostparamsasset record at the END of every
//    ApplyUpdate call (0x822C1450..0x822C15BC), store-for-store identical to the
//    block in Prepare @0x822C0FA0 (landed in partfile 16) -- a live-tuning re-read
//    that survived into the shipping build. All paths converge on it (the
//    "stop boosting" arm `b loc_822C1450` at 0x822C13B0 jumps INTO it), so it is
//    not a cold/debug-only branch. Feb-2007 read a single parameter, in Prepare.
//  * The drift anti-exploit block (mfTimeSpentCheating drop-off), the high-speed
//    bonus, the continuous-boosting bonus, mfTimeBoosting and the chain counter
//    are all post-Feb-2007 additions with no counterpart in that drop.
// ============================================================================


namespace BrnWorld
{

// ----------------------------------------------------------------------------
// DecFIGS DWARF BrnBoostBurnout2.cpp:43/44 declares these two at .cpp scope
// (BrnBoostBurnout2.h:83-87 records that they are NOT header declarations).
// Both are DOUBLY attested:
//   VALUE from the DWARF's own initialised bytes --
//     KF_MIN_TIME_TILL_DROP_OFF     = [65,160,0,0] = 0x41A00000 = 20.0f
//     KF_MIN_DISTANCE_TILL_DROP_OFF = [66,200,0,0] = 0x42C80000 = 200.0f
//   VALUE from the X360 image constants this body loads --
//     flt_820149CC (0x822C11E8) == 20.0f, flt_8201A1F0 (0x822C11C0) == 200.0f
// The drift anti-exploit block below is the only place in the whole TU that
// compares a time or a distance against a threshold, which is what binds each
// name to its site.
// ----------------------------------------------------------------------------
const f32 KF_MIN_TIME_TILL_DROP_OFF     = 20.0f;    // flt_820149CC
const f32 KF_MIN_DISTANCE_TILL_DROP_OFF = 200.0f;   // flt_8201A1F0

namespace
{
    // MERGE NOTE: BoostBurnout2_wP_16.cpp:56/61 carries this identical pair for
    // Prepare @0x822C0F38, which builds the same key with the same GUID. Both are
    // anonymous-namespace (internal linkage), so the two partfiles do NOT collide
    // as separate TUs -- but when the conductor merges the partfiles into one
    // BrnBoostBurnout2.cpp, keep exactly ONE copy of each.

    // The boostparamsasset collection GUID BoostBurnout2 tunes itself from.
    // X360-attested HERE too, not just in Prepare: ApplyUpdate @0x822C1450
    // `lis r3,8` + @0x822C145C `ori r3,r3,0xCA05` == 0x0008CA05 == 576005, handed
    // to ConvertI64ToA with r5 == 0xA so the key text is "576005".
    //
    // NAME AND TYPE are the DecFIGS DWARF's, verbatim -- BrnBoostBurnout2.cpp:4
    // of the dwarfdump declares `const int64_t KI_DEFAULT_DANGER_BOOST_PARAMS_
    // GUID = 576005;` at .cpp scope.
    const s64 KI_DEFAULT_DANGER_BOOST_PARAMS_GUID = 576005;

    // The stack text buffer ConvertI64ToA writes the decimal GUID into. Hex-Rays
    // renders it as `_BYTE v34[512]` at sp+0x70 of this function's 0x2B0-byte
    // frame. A char count is width-invariant, so this is not a console-size
    // hazard (nothing here feeds a stride, an advance or an allocation).
    const u32 KU_BOOST_KEY_TEXT_SIZE = 512;
}

// ---------------------------------------------------------------------------
// ApplyUpdate @ 0x822C1128 -- base vtable slot 1 (pure in the base).
//
// SIGNATURE. The prologue takes `this` in r3 and the time step in f1 only
// (`mr r31,r3` @0x822C1140, `fmr f31,f1` @0x822C1144; nothing in the body ever
// reads r4). PPC float-arg ABI: a float travels in an FPR and SKIPS its GPR
// slot, so the absent r4 is NOT a dropped pointer argument -- it confirms there
// is no second parameter, matching the DWARF `virtual void ApplyUpdate(float32_t)`.
//
// REGISTER MAP for the walk below: r31 = this, f31 = lfTimeStep,
// f29 = flt_82001CC0 = 0.0f, f30 = lfBoostIncrease, r30 = 0, r28 = 1,
// r29 = &flt_8201497C (the constant-pool anchor the `lfs (flt_XXXX -
// 0x8201497C)(r29)` loads use).
//
// VIRTUAL DISPATCHES. Every one is `lwz r11,0(r31) ; lwz r11,<off>(r11) ;
// mtctr ; bctrl`; the X360 vtable has 4-byte slots, so slot == off/4, and the
// slot numbers below are the base's 50-slot order (BrnBoostStrategy.h):
//   +0x50 -> slot 20 IsInAir      +0x54 -> slot 21 IsDrifting
//   +0x64 -> slot 25 IsOncoming   +0x6C -> slot 27 IsTailgating
//   +0xC4 -> slot 49 AddBoost
// BoostBurnout2 overrides none of these, so the unqualified calls below are that
// same virtual dispatch.
//
// NaN POLARITY. PPC `fcmpu` leaves LT/GT/EQ all clear when a compare is
// unordered, so a `ble`/`bge` guard is TAKEN on NaN while the C++ `<=`/`>=` it
// looks like is FALSE on NaN. Every guard below is written from the branch
// SENSE, using the negated ordered predicate where the asm's FALL-THROUGH (not
// its taken edge) is the C++ condition. Each such site is flagged inline.
// Likewise `fsel frD,frA,frB,frC` yields frC when frA is NaN (NaN is not >= 0),
// and treats -0.0 as >= 0; the two clamps below are written in the fsel's own
// ternary sense so both behaviours survive -- the same idiom the sibling
// BoostBurnout2_wP_05.cpp::OnWrecked already lands.
// ---------------------------------------------------------------------------
void
BoostBurnout2::ApplyUpdate(f32 lfTimeStep)
{
    f32 lfBoostIncrease = 0.0f;                        // 0x822C1154 fmr f30, f29

    // -- air ---------------------------------------------------------------
    // 0x822C1160 bctrl slot 20; 0x822C1170 lfs 0x18 / fmuls f30, f0, f31. The
    // asm multiplies INTO f30 rather than accumulating because it knows f30 is
    // still 0.0 here; the source accumulation is restored.
    if (IsInAir())
    {
        lfBoostIncrease += mfAirEarning * lfTimeStep;
    }

    // -- drift, with the chain anti-exploit drop-off ------------------------
    // 0x822C1188 bctrl slot 21. The plain earning is the `else` arm at
    // loc_822C122C; the attenuated arm only runs while a chain is actually being
    // milked (boosting AND more than one chain link already banked).
    if (IsDrifting())
    {
        // 0x822C11A0 lbz 0xC5 (mbBoosting) / 0x822C11AC lwz 0x13C (miChainSize).
        // `cmpwi cr6,r11,1` + `ble` is a SIGNED compare, so the attenuated arm
        // needs strictly `miChainSize > 1`.
        if (mbBoosting && miChainSize > 1)
        {
            // 0x822C11C4 fcmpu / bge loc_822C1224: `bge` is taken for GT, EQ
            // *and unordered*, and it leads to the reset arm -- so the earning
            // arm is the strictly-ordered-less case and plain `<` is exact.
            if (mfTotalDistanceTraveled < KF_MIN_DISTANCE_TILL_DROP_OFF)
            {
                // 0x822C11CC..0x822C11F8: accumulate, then clamp into
                // [0, KF_MIN_TIME_TILL_DROP_OFF] with two fsels. (The asm stores
                // to +0x98 twice, unclamped then clamped; nothing reads it in
                // between, so one store of the final value is equivalent.)
                f32 lfCheatTime = mfTimeSpentCheating + lfTimeStep;   // 0x822C11D4 fadds
                lfCheatTime = (-lfCheatTime >= 0.0f)                  // 0x822C11E4 fsel f13,f13,f29,f0
                                  ? 0.0f
                                  : lfCheatTime;
                lfCheatTime = ((KF_MIN_TIME_TILL_DROP_OFF - lfCheatTime) >= 0.0f) // 0x822C11F4 fsel f13,f11,f13,f0
                                  ? lfCheatTime
                                  : KF_MIN_TIME_TILL_DROP_OFF;
                mfTimeSpentCheating = lfCheatTime;                   // 0x822C11F8 stfs 0x98

                // 0x822C11FC..0x822C1218: (20 - t) * 0.05, clamped to [0, 1].
                // flt_8201497C is a SEPARATE image constant (0.050000001f), not
                // a compiler-folded reciprocal of the 20.0f above -- the asm
                // loads both -- so it is kept as its own constant, while being
                // numerically exactly 1 / KF_MIN_TIME_TILL_DROP_OFF.
                // Name not recovered (the DWARF attests only the three .cpp
                // constants above); the VALUE is the asm's.
                const f32 KF_DROP_OFF_RATE = 0.05f;                  // flt_8201497C

                f32 lfDropOff = (KF_MIN_TIME_TILL_DROP_OFF - lfCheatTime) * KF_DROP_OFF_RATE;
                lfDropOff = (-lfDropOff >= 0.0f)                     // 0x822C120C fsel f0,f13,f29,f0
                                ? 0.0f
                                : lfDropOff;
                lfDropOff = ((1.0f - lfDropOff) >= 0.0f)             // 0x822C1218 fsel f0,f11,f0,f13
                                ? lfDropOff
                                : 1.0f;                              // flt_82001C98

                // 0x822C121C fmuls f0, f0, f12 (mfDriftEarning), then the shared
                // 0x822C1230 fmadds f30, f0, f31, f30. Grouping preserved.
                lfBoostIncrease += lfDropOff * mfDriftEarning * lfTimeStep;
            }
            else
            {
                // 0x822C1224 stfs f29, 0x98 then `b loc_822C1234` -- the drift
                // earning is SKIPPED entirely on this arm, not merely
                // un-attenuated. Deliberate: the jump target is PAST the shared
                // fmadds, and this is the only path that leaves f30 untouched.
                mfTimeSpentCheating = 0.0f;
            }
        }
        else
        {
            // loc_822C122C: full, unattenuated drift earning.
            lfBoostIncrease += mfDriftEarning * lfTimeStep;
        }
    }

    // -- oncoming ----------------------------------------------------------
    // 0x822C1244 bctrl slot 25; 0x822C1254 lfs 0x78 / fmadds f30, f0, f31, f30.
    if (IsOncoming())
    {
        lfBoostIncrease += mfBoostOnComing * lfTimeStep;
    }

    // -- tailgating --------------------------------------------------------
    // 0x822C126C bctrl slot 27; 0x822C1280 lfs 0x44 / 0x822C1288 fmuls f1,f0,f31
    // / 0x822C1294 bctrl slot 49. Tailgating is awarded DIRECTLY through AddBoost
    // (so it is subject to AddBoost's speed multiplier and earning gate) instead
    // of joining lfBoostIncrease -- keep it out of the accumulator.
    if (IsTailgating())
    {
        AddBoost(mfTailgatingEarning * lfTimeStep);
    }

    // -- high-speed bonus --------------------------------------------------
    // 0x822C1298 lfs 0xB4 / 0x822C12A0 fcmpu flt_82014808 / 0x822C12A4 ble skip.
    // `ble` is taken for LT, EQ and unordered, so the executed arm is the
    // strictly-ordered-greater case and plain `>` is exact.
    // Neither constant is named in the DWARF (the six KF_DANGER_BOOST_* statics
    // it attests are header-scope and belong to the near-miss / crash-escape
    // paths, not here), so these are local names over the asm's VALUES, which
    // Hex-Rays renders directly as `if ( *(a1 + 180) > 100.0 ) ... (a2 * 1.1)`.
    {
        const f32 KF_HIGH_SPEED_THRESHOLD = 100.0f;   // flt_82014808
        const f32 KF_HIGH_SPEED_EARNING   = 1.1f;     // flt_82004A1C

        if (mfSpeed > KF_HIGH_SPEED_THRESHOLD)
        {
            // 0x822C12B0 fmadds f30, f31, f0, f30 -- operand order preserved.
            lfBoostIncrease += lfTimeStep * KF_HIGH_SPEED_EARNING;
        }
    }

    // -- continuous-boosting bonus -----------------------------------------
    // 0x822C12B4 lbz r10, 0xC5. That single load is reused for the chain-mode
    // clear and the hidden-boost branch below -- nothing between them writes
    // mbBoosting. (The compiler tests it two ways off that one byte:
    // `cmplwi r10,1`+`bne` here and `cmplwi r10,0`+`beq` at 0x822C12F4. For the
    // `bool` member the DWARF declares, those are the same test.)
    {
        const f32 KF_CONTINUOUS_BOOST_REFERENCE_TIME = 60.0f;   // flt_82004C6C

        if (mbBoosting)
        {
            mfContinuousBoostingTimeAdd += lfTimeStep;          // 0x822C12C8/0x822C12CC

            // 0x822C12D4 fcmpu / 0x822C12D8 bgt loc_822C12F0. The divide is on
            // the FALL-THROUGH edge, which is taken for LT, EQ *and unordered*.
            // Spelled `<=` this would skip the divide on NaN; the negated
            // ordered form reproduces the hardware.
            if (!(mfContinuousBoostingTimeAdd > KF_CONTINUOUS_BOOST_REFERENCE_TIME))
            {
                // 0x822C12DC fdivs f0, f13, f0 -- reference / elapsed.
                mfTimeBasedBoostingBonus =
                    KF_CONTINUOUS_BOOST_REFERENCE_TIME / mfContinuousBoostingTimeAdd;
            }
        }
        else
        {
            // loc_822C12E8, in emitted order.
            mfContinuousBoostingTimeAdd = 0.0f;                 // 0x822C12E8 stfs 0x144
            mfTimeBasedBoostingBonus    = 0.0f;                 // 0x822C12EC stfs 0x140
        }
    }

    // 0x822C12F8 stb r30, 0xC9 -- cleared unconditionally here, and re-armed
    // only by the run gate below.
    mbInChainMode = false;

    // 0x822C12FC beq: while boosting the earnings are stashed instead of being
    // credited, and are paid out as the chain bonus when the run ends.
    if (mbBoosting)
    {
        mfHiddenBoost += lfBoostIncrease;                       // 0x822C1300..0x822C1308
    }
    else
    {
        AddBoost(lfBoostIncrease);                              // 0x822C1324, slot 49
    }

    // -- the boost run gate ------------------------------------------------
    // 0x822C1328 reloads mbBoosting into r9 (the AddBoost call above forced the
    // reload; AddBoost does not write it, so the value is unchanged).
    //
    // lbBoostRunYoung: 0x822C1338 lfs 0x148 / 0x822C1340 lfs flt_820147F8 /
    // fcmpu / `blt` KEEPS r11 = 1, otherwise loc_822C134C sets r11 = 0. `blt` is
    // taken only on an ordered LT, so plain `<` is exact (an unordered compare
    // falls to the r11 = 0 arm, exactly as C++ `<` yields false on NaN).
    // Name not recovered; the value is Hex-Rays' rendering of flt_820147F8.
    const f32 KF_BOOST_RUN_GRACE_TIME = 1.25f;                  // flt_820147F8
    const bool lbBoostRunYoung = mbBoosting && (mfTimeBoosting < KF_BOOST_RUN_GRACE_TIME);

    // 0x822C1350 lfs 0xA0 / 0x822C1358 lfs 0xA4 / fcmpu / `beq` keeps r11 = 1 --
    // an exact ordered equality against the maximum is what marks "a fresh full
    // bar", and it is published to mbIsBoostFull (0x822C1378 stb 0xC3) BEFORE
    // the gate runs.
    const bool lbBoostFull = (mfBoostAmount == mfMaxBoost);
    mbIsBoostFull = lbBoostFull;

    // 0x822C1370..0x822C13A4, three conjuncts:
    //   (mbBoostRequested || lbBoostRunYoung)  -- 0x822C1370 lbz 0xC7 / bne, then
    //       the saved lbBoostRunYoung byte / beq. The grace term is what lets a
    //       chain re-ignite for 1.25 s after the player releases the button;
    //   mfBoostAmount > 0.0f                   -- 0x822C138C fcmpu f13,f29 /
    //       `ble` skips to the else arm, so this is an ordered GT;
    //   (mbBoosting || lbBoostFull)            -- 0x822C1394 / 0x822C139C. A NEW
    //       run may only start off a completely full bar (the Burnout-2 rule);
    //       an existing run continues regardless.
    if ((mbBoostRequested || lbBoostRunYoung)
        && mfBoostAmount > 0.0f
        && (mbBoosting || lbBoostFull))
    {
        mbInChainMode = true;                                   // 0x822C13B8 stb 0xC9

        if (lbBoostFull)
        {
            // 0x822C13C4..0x822C13D0 -- start of a fresh run off a full bar,
            // in emitted order.
            mfHiddenBoost      = 0.0f;                          // 0x822C13C4
            mbBoosting         = true;                          // 0x822C13C8
            mfTimeBoosting     = 0.0f;                          // 0x822C13CC
            mbBoostInterrupted = false;                         // 0x822C13D0
        }

        mfTimeBoosting += lfTimeStep;                           // 0x822C13DC/0x822C13E0

        // 0x822C13D8 lfs 0xF0 / 0x822C13E4 fcmpu f29 / `bne` skips the fallback
        // load at 0x822C13EC: the per-car boost loss level overrides the tuned
        // burn rate whenever it is non-zero. `== 0.0f` is exact -- `bne` is taken
        // for an unordered compare too, and C++ `==` is likewise false on NaN.
        f32 lfBurnRate = mfCurrentCarBoostLossLevel;
        if (lfBurnRate == 0.0f)
        {
            lfBurnRate = mfBurnRateBoost;
        }

        // 0x822C13F0 fnmsubs f0, f0, f31, f13 (== mfBoostAmount - rate*dt; f13
        // still holds the +0xA0 value loaded at 0x822C1350, and nothing has
        // written the member since) + 0x822C13F4 stfs 0xA0 + 0x822C13F8 fcmpu /
        // `bge` / 0x822C1400 stfs 0.0. That subtract-then-clamp-at-zero pair IS
        // BoostStrategy::RemoveBoost (DecFIGS BrnBoostStrategy.cpp:168; no
        // standalone X360 symbol -- inlined at every call site, exactly as in
        // OnCrash @0x822A6370 and OnWrecked @0x822C1608). Reversing the inline
        // restores the call, matching the landed sibling partfile 05.
        RemoveBoost(lfBurnRate * lfTimeStep);

        // 0x822C1404 reloads 0xA0 / 0x822C1408 fcmpu f29 / 0x822C140C `bgt`
        // jumps past the run-ended arm. The arm is therefore the FALL-THROUGH,
        // taken for LT, EQ *and unordered* -- so the negated ordered predicate,
        // not `<= 0.0f`.
        if (!(mfBoostAmount > 0.0f))
        {
            mbBoosting = false;                                 // 0x822C1414 stb 0xC5

            // 0x822C1410 lbz 0x138 / 0x822C141C bne skips the payout: a run that
            // was interrupted pays NO chain bonus and does NOT advance the chain
            // counter. OnCrash @0x822A639C sets the flag to 1 precisely to
            // suppress this path.
            if (!mbBoostInterrupted)
            {
                // 0x822C1430 fadds f1, 0x134, 0x90 then 0x822C143C bctrl slot 49.
                AddBoost(mfHiddenBoost + mfBoostChainBonus);
                mbChainNotifyPending = true;                    // 0x822C1444 stb 0xCB
                ++miChainSize;                                  // 0x822C1448/0x822C144C
            }
        }
    }
    else
    {
        // loc_822C13A8 -- the run is over and the chain is broken.
        mbBoosting  = false;                                    // 0x822C13A8 stb 0xC5
        miChainSize = 0;                                        // 0x822C13AC stw 0x13C
    }

    // -- the per-frame tuning re-read (0x822C1450..0x822C15BC) --------------
    // Store-for-store identical to the block in Prepare @0x822C0FA0 (partfile
    // 16). Every path reaches it, including the "run is over" arm above, which
    // branches straight into it at 0x822C13B0.
    //
    // The key handed to the boostparamsasset ctor is LIVE, not dead: the ctor
    // @0x822B8C88 never WRITES r4, so the caller's key reaches
    // Attrib::FindCollection whole as the COLLECTION key (full derivation in
    // boostparamsasset.h). Attrib::StringToKey returns u64 and 0x822C146C is a
    // full 64-bit `mr r4,r3` with no clrldi -- so there is deliberately NO
    // narrowing cast on it here. The owner argument is `li r5,0` @0x822C1470,
    // which is the accessor's default.
    //
    // SpeedForMin/MaxEarning are the ONLY two attributes converted int->float
    // (lwz + extsw + std/lfd + fcfid + frsp at 0x822C1498 and 0x822C14B4); they
    // are attrib Int32 and land in the f32 members at +0x1C/+0x20.
    {
        char lacBoostKey[KU_BOOST_KEY_TEXT_SIZE];
        rw::core::stdc::ConvertI64ToA(KI_DEFAULT_DANGER_BOOST_PARAMS_GUID, lacBoostKey, 10);

        Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacBoostKey));

        // ---- the 34 tuning params, in member order (== asm store order) -----
        mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();     // +0x10 <- rec+0x3C
        mfDriftEarning            = lBoostParams.DriftEarning();             // +0x14 <- rec+0x54
        mfAirEarning              = lBoostParams.AirEarning();               // +0x18 <- rec+0x84
        mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning()); // +0x1C <- rec+0x1C (Int32, fcfid)
        mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning()); // +0x20 <- rec+0x20 (Int32, fcfid)
        mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();    // +0x24 <- rec+0x40
        mfTakedownEarning         = lBoostParams.TakedownEarning();          // +0x28 <- rec+0x08
        mfShuntEarning            = lBoostParams.ShuntEarning();             // +0x2C <- rec+0x28
        mfSlamEarning             = lBoostParams.SlamEarning();              // +0x30 <- rec+0x24
        mfNudgeEarning            = lBoostParams.NudgeEarning();             // +0x34 <- rec+0x38
        mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();      // +0x38 <- rec+0x04
        mfGrindingEarning         = lBoostParams.GrindingEarning();          // +0x3C <- rec+0x4C
        mfRubbingEarning          = lBoostParams.RubbingEarning();           // +0x40 <- rec+0x2C
        mfTailgatingEarning       = lBoostParams.TailgatingEarning();        // +0x44 <- rec+0x0C
        mfTrafficCheck            = lBoostParams.TrafficCheck();             // +0x48 <- rec+0x00
        mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();        // +0x4C <- rec+0x6C
        mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();      // +0x50 <- rec+0x48
        mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();      // +0x54 <- rec+0x44
        mfAirSpinEarning          = lBoostParams.AirSpinEarning();           // +0x58 <- rec+0x80
        mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();        // +0x5C <- rec+0x7C
        mfCleanLanding            = lBoostParams.CleanLanding();             // +0x60 <- rec+0x60
        mfFakieLanding            = lBoostParams.FakieLanding();             // +0x64 <- rec+0x50
        mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();        // +0x68 <- rec+0x68
        mfComboModifier           = lBoostParams.ComboModifier();            // +0x6C <- rec+0x5C
        mfBurnRateBoost           = lBoostParams.BurnRateBoost();            // +0x70 <- rec+0x64
        mfBoostChainMin           = lBoostParams.BoostChainMin();            // +0x74 <- rec+0x70
        mfBoostOnComing           = lBoostParams.OnComing();                 // +0x78 <- rec+0x34
        mfBeingSlammed            = lBoostParams.BeingSlammed();             // +0x7C <- rec+0x78
        mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();         // +0x80 <- rec+0x14
        mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();        // +0x84 <- rec+0x10
        mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();    // +0x88 <- rec+0x18
        mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();  // +0x8C <- rec+0x58
        mfBoostChainBonus         = lBoostParams.BoostChainBonus();          // +0x90 <- rec+0x74
        mfOnWrecked               = lBoostParams.OnWrecked();                // +0x94 <- rec+0x30

        // lBoostParams leaves scope here -- the `bl Attrib::Instance::~Instance`
        // at 0x822C15BC. Nothing is returned: the asm's r3 at that point is the
        // destructor's own dead value, and the DWARF declares `void`.
    }
}

}   // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout2_wP_16.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout2 -- wave P partfile 16.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX; the
// ARTIST asm is rung 1 and arbitrates every branch, constant and store below):
//   BoostBurnout2::Prepare          @ 0x822C0F38   (base vtable slot 0)
//   BoostBurnout2::UpdateStuntBoost @ 0x822A6478   (base vtable slot 48)
//
// Both were parked earlier in wave P on two header blockers that are now gone:
//   * Attrib::Gen::boostparamsasset now exposes the 34 named tuning-parameter
//     accessors (GameSource/AttribSys/Generated/classes/boostparamsasset.h) and
//     its constructor key is u64, so the collection actually resolves. This TU
//     goes through those accessors -- it does NOT re-export or poke
//     Instance::GetLayoutPointer.
//   * BrnGameState::GameStateModuleIO::CompletedStuntAction is now a complete
//     type in GameSource/GameState/BrnGameActions.h, with the X360 field order
//     (miCompletedBarrelRolls at +0x18, mbSuccessfulLanding at +0x1C).
//
// Offset -> member map used below. BoostBurnout2's own members start at +0x130
// because sizeof(BoostStrategy) == 0x130 on BOTH console and host; the 34
// tuning params and the two flags live in the base (BrnBoostStrategy.h).
//   +0x010..+0x094 BoostStrategy:: the 34 tuning params (see Prepare)
//   +0x0C3 BoostStrategy::mbIsBoostFull
//   +0x0CB BoostStrategy::mbChainNotifyPending
//   +0x138 BoostBurnout2::mbBoostInterrupted
//   +0x13C BoostBurnout2::miChainSize
//   +0x140 BoostBurnout2::mfTimeBasedBoostingBonus
//   +0x144 BoostBurnout2::mfContinuousBoostingTimeAdd
//   +0x148 BoostBurnout2::mfTimeBoosting
// ============================================================================


namespace BrnWorld
{

namespace
{
// (fold: an identical definition of KI_DEFAULT_DANGER_BOOST_PARAMS_GUID was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KU_BOOST_KEY_TEXT_SIZE was dropped here -- this TU defines it once, above)
}

// ---------------------------------------------------------------------------
// Prepare @ 0x822C0F38 -- base vtable slot 0.
//
// Shape, walked end to end from the asm:
//   0x822C0F4C  bl BoostStrategy::Prepare ; clrlwi r11,r3,24 ; cmplwi ; bne
//               -> continue, else `li r3,0 ; blr`          # EARLY-OUT on false
//   0x822C0F74  r3 = 0x0008CA05 (576005) ; r5 = 0xA ; r4 = sp+0x70 ;
//               bl rw__core__stdc__ConvertI64ToA           # -> "576005"
//   0x822C0F8C  bl Attrib__StringToKey                     # r3 = the FULL u64
//   0x822C0F90  mr r4,r3 (64-bit move, no clrldi) ; li r5,0 ; r3 = sp+0x60 ;
//               bl Attrib::Gen::boostparamsasset::boostparamsasset
//   0x822C0FA0  lwz r11, 0x64(sp)   # the constructed instance's layout block
//                                   # (Attrib::Instance::mpAttributeData)
//   0x822C0FA4..0x822C10F0  34 record loads -> 34 base-member stores
//   0x822C10E4..0x822C1104  the resets (interleaved with the last stores by the
//                           scheduler; all independent)
//   0x822C1108  bl Attrib::Instance::~Instance   # lBoostParams leaves scope
//   0x822C110C  li r3,1 ; blr                    # return true
//
// RETAIL vs FEB-2007 -- the asm wins on all of these:
//  * Feb-2007 (BrnBoostBurnout2.cpp:53-63) calls BoostStrategy::Prepare() and
//    IGNORES its result. Retail EARLY-OUTS on it (the byte-narrowing
//    `clrlwi r11,r3,24` + `bne` at 0x822C0F50-0x822C0F58, with the `li r3,0`
//    return in the not-taken path).
//  * Feb-2007 loads exactly ONE attribute here (Burnout2CrashDecrease) because
//    its BASE Prepare loaded the shared ones. Retail's base loads NOTHING, so
//    all 34 tuning params are loaded HERE.
//  * Feb-2007 assigns `mfCrashDecrease = lBoostParams.Burnout2CrashDecrease();`.
//    RETAIL DOES NOT: there is no store to +0x130 anywhere in the image --
//    OnCrash @0x822A6378 only READS it (BrnBoostBurnout2.h records the same).
//    Do not re-add the assignment.
//  * Feb-2007 clears mbBoostInterrupted BEFORE the asset load; retail clears it
//    after (0x822C10E4). Semantically identical; asm order kept.
//
// The key handed to the boostparamsasset ctor is LIVE, not dead: the ctor
// @0x822B8C88 never WRITES r4, so it reaches Attrib::FindCollection whole as the
// COLLECTION key (full derivation in boostparamsasset.h). Attrib::StringToKey
// returns u64 and the asm's `mr r4,r3` is a full 64-bit move with no clrldi --
// so there is deliberately NO narrowing cast on it here.
//
// SpeedForMin/MaxEarning are the ONLY two attributes converted int->float
// (lwz + extsw + std/lfd + fcfid + frsp at 0x822C0FBC and 0x822C0FD8); they are
// attrib Int32 and land in the f32 members at +0x1C/+0x20, the two values
// AddBoost @0x822C0E10 lerps between.
// ---------------------------------------------------------------------------
bool
BoostBurnout2::Prepare()
{
    if (!BoostStrategy::Prepare())
    {
        return false;
    }

    char lacBoostKey[KU_BOOST_KEY_TEXT_SIZE];
    rw::core::stdc::ConvertI64ToA(KI_DEFAULT_DANGER_BOOST_PARAMS_GUID, lacBoostKey, 10);

    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacBoostKey));

    // ---- the 34 tuning params, in member order (== asm store order) ---------
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();     // +0x10 <- rec+0x3C
    mfDriftEarning            = lBoostParams.DriftEarning();             // +0x14 <- rec+0x54
    mfAirEarning              = lBoostParams.AirEarning();               // +0x18 <- rec+0x84
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning()); // +0x1C <- rec+0x1C (Int32, fcfid)
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning()); // +0x20 <- rec+0x20 (Int32, fcfid)
    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();    // +0x24 <- rec+0x40
    mfTakedownEarning         = lBoostParams.TakedownEarning();          // +0x28 <- rec+0x08
    mfShuntEarning            = lBoostParams.ShuntEarning();             // +0x2C <- rec+0x28
    mfSlamEarning             = lBoostParams.SlamEarning();              // +0x30 <- rec+0x24
    mfNudgeEarning            = lBoostParams.NudgeEarning();             // +0x34 <- rec+0x38
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();      // +0x38 <- rec+0x04
    mfGrindingEarning         = lBoostParams.GrindingEarning();          // +0x3C <- rec+0x4C
    mfRubbingEarning          = lBoostParams.RubbingEarning();           // +0x40 <- rec+0x2C
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();        // +0x44 <- rec+0x0C
    mfTrafficCheck            = lBoostParams.TrafficCheck();             // +0x48 <- rec+0x00
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();        // +0x4C <- rec+0x6C
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();      // +0x50 <- rec+0x48
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();      // +0x54 <- rec+0x44
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();           // +0x58 <- rec+0x80
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();        // +0x5C <- rec+0x7C
    mfCleanLanding            = lBoostParams.CleanLanding();             // +0x60 <- rec+0x60
    mfFakieLanding            = lBoostParams.FakieLanding();             // +0x64 <- rec+0x50
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();        // +0x68 <- rec+0x68
    mfComboModifier           = lBoostParams.ComboModifier();            // +0x6C <- rec+0x5C
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();            // +0x70 <- rec+0x64
    mfBoostChainMin           = lBoostParams.BoostChainMin();            // +0x74 <- rec+0x70
    mfBoostOnComing           = lBoostParams.OnComing();                 // +0x78 <- rec+0x34
    mfBeingSlammed            = lBoostParams.BeingSlammed();             // +0x7C <- rec+0x78
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();         // +0x80 <- rec+0x14
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();        // +0x84 <- rec+0x10
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();    // +0x88 <- rec+0x18
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();  // +0x8C <- rec+0x58
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();          // +0x90 <- rec+0x74
    mfOnWrecked               = lBoostParams.OnWrecked();                // +0x94 <- rec+0x30

    // ---- the resets --------------------------------------------------------
    // asm 0x822C10E4..0x822C1104, in emitted order:
    //   stb  r10, 0x138(r31)   stw  r10, 0x13C(r31)   stb  r10, 0xCB(r31)
    //   stfs f0,  0x140(r31)   stb  r10, 0xC3(r31)    stfs f0, 0x144(r31)
    //   stfs f0,  0x148(r31)                          (f0 == flt_82001CC0 == 0.0f)
    // Note that +0x130 mfCrashDecrease and +0x134 mfHiddenBoost are NOT touched.
    mbBoostInterrupted          = false;   // +0x138
    miChainSize                 = 0;       // +0x13C
    mbChainNotifyPending        = false;   // +0x0CB (base)
    mfTimeBasedBoostingBonus    = 0.0f;    // +0x140
    mbIsBoostFull               = false;   // +0x0C3 (base)
    mfContinuousBoostingTimeAdd = 0.0f;    // +0x144
    mfTimeBoosting              = 0.0f;    // +0x148

    // lBoostParams leaves scope here -- the `bl Attrib::Instance::~Instance`
    // at 0x822C1108, immediately before `li r3,1`.
    return true;
}

// ---------------------------------------------------------------------------
// UpdateStuntBoost @ 0x822A6478 -- base vtable slot 48.
//
// Four independent stunt awards, each gated on its own bit of
// lpCompletedStuntAction->muStuntActionComplete and each paid through the
// VIRTUAL AddBoost (base slot 49, vtable +0xC4 -- every one of the four sites is
// `lwz r11,0(r31) ; lwz r11,0xC4(r11) ; mtctr ; bctrl`, never a direct call), so
// BoostBurnout2's own AddBoost @0x822C0E10 speed-lerp + clamp applies to every
// award. An unqualified call on `this` is that virtual dispatch.
//
//   bit 0 (clrlwi  r11,r11,31)      @0x822A64C0
//        -> AddBoost((f32)miCompletedBarrelRolls * mfBarrelRollEarning)
//           the count is an INT: `lwz r11,0x18(r30)` + extsw + fcfid + frsp
//           (0x822A64CC-0x822A64F0) before `fmuls f1,f13,f0` -- f13 is the
//           converted count, f0 is `lfs f0,0x5C(r31)` == mfBarrelRollEarning.
//   bit 1 (rlwinm r11,r11,0,30,30)  @0x822A6504
//        -> AddBoost(mfAirSpinEarning * mfCompletedAirSpinAngle)
//           `lfs f0,0x58(r31)` * `lfs f13,8(r30)`, fmuls f1,f0,f13.
//   bit 2 (rlwinm r11,r11,0,29,29)  @0x822A6534
//        -> AddBoost(mfHandbrake180Earning * mfCompletedHandbreakTurnAngle)
//           `lfs f0,0x50(r31)` * `lfs f13,0xC(r30)`, fmuls f1,f0,f13.
//   bit 3 (rlwinm r11,r11,0,28,28)  @0x822A6564
//        -> AddBoost(mfCleanLanding)   flat, no payload: `lfs f1,0x60(r31)`.
//
// Operand order in each fmuls is preserved exactly as encoded. The bits are NOT
// mutually exclusive -- each `beq` skips only its own block and falls through
// into the next test, so one action can pay all four.
//
// The four masks 1/2/4/8 are BrnPhysics::EStuntActionComplete's BARREL_ROLL /
// AIR_SPIN / HANDBREAK_TURN / CLEANLANDING (DWARF-verbatim enum, already
// recovered in BrnStuntOffencesManagerShared.h) -- the same enum
// CompletedStuntAction::muStuntActionComplete is documented to carry. No names
// are invented here.
//
// The assert is the retail one: expression "lpCompletedStuntAction != NULL",
// file "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/EntityModules/
// RaceCarEntityModule/Boost/BrnBoostBurnout2.cpp", line 0x304 == 772. It is NOT
// an early-out -- 0x822A64B8 falls straight through into `lwz r11,0(r30)`, so no
// `return` is added after it.
//
// No Feb-2007 counterpart: UpdateStuntBoost did not exist in that drop. This
// body is read entirely from the retail asm.
// ---------------------------------------------------------------------------
void
BoostBurnout2::UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction)
{
    CGS_ASSERT(lpCompletedStuntAction != NULL, "lpCompletedStuntAction != NULL");

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_BARREL_ROLL)
    {
        AddBoost(static_cast<f32>(lpCompletedStuntAction->miCompletedBarrelRolls) * mfBarrelRollEarning);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_AIR_SPIN)
    {
        AddBoost(mfAirSpinEarning * lpCompletedStuntAction->mfCompletedAirSpinAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_HANDBREAK_TURN)
    {
        AddBoost(mfHandbrake180Earning * lpCompletedStuntAction->mfCompletedHandbreakTurnAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_CLEANLANDING)
    {
        AddBoost(mfCleanLanding);
    }
}

}   // namespace BrnWorld
