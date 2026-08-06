#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)

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
