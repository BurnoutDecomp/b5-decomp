#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 01.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only -- and it disagrees with
// retail in several places called out per function below).
//
// Functions in this partfile:
//   BoostBurnout5::AreWeAllowedToBoost   @ 0x822A6BD8   (vtable slot 47)
//   BoostBurnout5::BlueModeRequestBoost  @ 0x822A6CA8   (private, non-virtual)
//
// A third function was assigned to this partfile -- BoostBurnout5::ApplyUpdate
// @0x822C1FC8 (slot 1). It is BLOCKED and parked out of tree, COMPLETE, at
// scratchpad/waveP/parked/BoostBurnout5_wP_1.parked.cpp: its tail re-reads all 34
// Attrib::Gen::boostparamsasset tuning parameters, and the committed generated
// class (GameSource/AttribSys/Generated/classes/boostparamsasset.h) derives
// `private Instance` and exposes NOTHING but its constructor -- no attribute
// accessors and no `using Instance::GetLayoutPointer;`. There is no way to reach
// the record without editing that header, which this pass may not do. (Identical
// blocker to BoostBurnout2::ApplyUpdate and BoostBurnout3::Prepare this wave; the
// one-line fix is quoted in the parked file's banner.)
//
// Offset -> member map used below. The base layout is the wave-P keystone header
// BrnBoostStrategy.h; BoostBurnout5's own members start at +0x130 because
// sizeof(BoostStrategy) == 0x130 on BOTH console and host, and every B5 offset
// used here is pinned by BoostBurnout5::Prepare @0x822C1D58 (its init stores walk
// +304/+308/+309/+310/+311/+312/+316/+320/+324/+328 = 0x130..0x148 in DWARF
// declaration order, 10-for-10):
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0A4 BoostStrategy::mfMaxBoost
//   +0x0C5 BoostStrategy::mbBoosting
//   +0x0CB BoostStrategy::mbChainNotifyPending      (Prepare stb 0 -> +203)
//   +0x100 BoostStrategy::mfMinBoostAllowedAmount
//   +0x130 BoostBurnout5::meBoostMode               (Prepare stw 0 -> +304, DWARF h:149)
//   +0x134 BoostBurnout5::mbLeaveBlueDueToCrash     (DWARF h:151)
//   +0x135 BoostBurnout5::mbLeaveBlueDueToInsufficientHidden (DWARF h:152)
//   +0x13C BoostBurnout5::mfHiddenBoost             (Prepare stfs 0.0 -> +316, DWARF h:163)
//   +0x140 BoostBurnout5::mbBoostInterrupted        (Prepare stb 0 -> +320, DWARF h:164)
//   +0x144 BoostBurnout5::miChainSize               (Prepare stw 0 -> +324, DWARF h:165)
//
// Independent semantic confirmation of that bool triple: BlueModeRequestBoost
// READS +0x134 exactly where Feb-2007 reads mbLeaveBlueDueToCrash, and SETS +0x135
// exactly where Feb-2007 sets mbLeaveBlueDueToInsufficientHidden -- two anchors
// that fall out of the DWARF order without being assumed by it.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// AreWeAllowedToBoost @ 0x822A6BD8  (vtable slot 47; pure in the base)
//
//   0x822A6BD8  lwz    r11, 0x130(r3)      # meBoostMode
//   0x822A6BDC  cmpwi  cr6, r11, 0
//   0x822A6BE0  bne    cr6, loc_822A6C24   # -> not E_BOOSTMODE_B3_RED
//   0x822A6BE4  lbz    r11, 0xC5(r3)       # mbBoosting
//   0x822A6BE8  cmplwi cr6, r11, 1
//   0x822A6BEC  bne    cr6, loc_822A6C0C   # -> not-boosting arm
//   0x822A6BF0  lis    r11, flt_82001CC0@ha
//   0x822A6BF4  lfs    f13, 0xA0(r3)       # mfBoostAmount
//   0x822A6BF8  lfs    f0,  flt_82001CC0@l # 0.0f
//   0x822A6BFC  fcmpu  cr6, f13, f0
//   0x822A6C00  ble    cr6, loc_822A6C44   # -> return false
//   0x822A6C04  li     r3, 1
//   0x822A6C08  blr
//   0x822A6C0C  lfs    f0,  0xA0(r3)       # mfBoostAmount
//   0x822A6C10  lfs    f13, 0x100(r3)      # mfMinBoostAllowedAmount
//   0x822A6C14  fcmpu  cr6, f0, f13
//   0x822A6C18  ble    cr6, loc_822A6C44   # -> return false
//   0x822A6C1C  li     r3, 1
//   0x822A6C20  blr
//   0x822A6C24  cmpwi  cr6, r11, 1         # E_BOOSTMODE_B2_BLUE?
//   0x822A6C28  bne    cr6, loc_822A6C44   # -> return false (neither mode)
//   0x822A6C2C  lis    r11, flt_82001CC0@ha
//   0x822A6C30  lfs    f13, 0xA0(r3)       # mfBoostAmount
//   0x822A6C34  li     r3, 1
//   0x822A6C38  lfs    f0,  flt_82001CC0@l # 0.0f
//   0x822A6C3C  fcmpu  cr6, f13, f0
//   0x822A6C40  bgtlr  cr6                 # return true iff ordered-greater
//   0x822A6C44  li     r3, 0
//   0x822A6C48  blr
//
// The mode is tested against BOTH enumerators (cmpwi 0, then cmpwi 1) with a
// shared `return false` fallthrough -- an if/else-if chain over the enum, not a
// two-way test, so the fallthrough arm is reproduced even though today's BoostMode
// has exactly two enumerators. (Contrast IsBlueMode @0x822A6C50, which really is a
// single equality.)
//
// Red mode is the segmented ("chunk") bar: a run in progress continues while any
// boost at all remains, but a NEW run may only start once the bar is above
// mfMinBoostAllowedAmount -- the one-chunk floor SetCarStatBoostLevel @0x822D5304
// computes from mfMaxBoost. Blue mode has no chunk floor: any boost will do. This
// is the same three-arm shape as BoostBurnout2::AreWeAllowedToBoost @0x822A6408,
// keyed on meBoostMode instead of mbInChainMode.
//
// NaN polarity: every true-exit is the ORDERED-greater case (`ble` falls through
// to `li r3,0`; `bgtlr` returns only on GT), which is exactly what C++ `>` already
// means for an unordered compare. No inversion needed.
//
// `cmplwi r11, 1` against mbBoosting is the compiler's choice for a bool known to
// hold 0/1 (the same TU uses `cmplwi ...,0` for the identical test at
// ApplyUpdate @0x822C2114); it is not a three-valued test.
//
// Feb-2007 has NO AreWeAllowedToBoost on BoostBurnout5 at all -- the whole
// per-mode gate post-dates that drop, so nothing here is ported from it.
// ---------------------------------------------------------------------------
bool
BoostBurnout5::AreWeAllowedToBoost()
{
    if (meBoostMode == E_BOOSTMODE_B3_RED)
    {
        if (mbBoosting)
        {
            return mfBoostAmount > 0.0f;
        }

        return mfBoostAmount > mfMinBoostAllowedAmount;
    }
    else if (meBoostMode == E_BOOSTMODE_B2_BLUE)
    {
        return mfBoostAmount > 0.0f;
    }

    return false;
}

// ---------------------------------------------------------------------------
// BlueModeRequestBoost @ 0x822A6CA8  (private non-virtual; DWARF h:195 /
// BrnBoostBurnout5.cpp:347. Its only caller is ApplyUpdate @0x822C2210.)
//
//   0x822A6CC0  lwz    r11, 0x130(r29)     # meBoostMode
//   0x822A6CC4  cmpwi  cr6, r11, 1
//   0x822A6CC8  beq    cr6, loc_822A6D40   # assert-if-not-blue, then continue
//   0x822A6CCC  bl     CgsDev__Assert__BeginAssert
//   ...          StrStream the literal into CgsDev::Assert::gpcMessageBuffer...
//   0x822A6D2C  li     r5, 0x15D           # line 349
//   0x822A6D38  bl     CgsDev__Assert__FireAssert
//   0x822A6D3C  bl     CgsDev__Assert__EndAssert
//   0x822A6D40  lbz    r11, 0x134(r29)     # mbLeaveBlueDueToCrash
//   0x822A6D48  beq    cr6, loc_822A6D80
//   0x822A6D50  lfs    f1,  0x13C(r29)     # mfHiddenBoost
//   0x822A6D58  lwz    r11, 0xC4(r11)      # vtable slot 49 == AddBoost
//   0x822A6D60  bctrl
//   0x822A6D68  li     r3, 0               # return false
//   0x822A6D70  stfs   f0,  0x13C(r29)     # mfHiddenBoost = 0.0f
//   0x822A6D80  lfs    f0,  0xA0(r29)      # mfBoostAmount
//   0x822A6D88  lfs    f13, 0xA4(r29)      # mfMaxBoost
//   0x822A6D8C  fcmpu  cr6, f0, f13
//   0x822A6D90  bne    cr6, loc_822A6DB4
//   0x822A6D98  li     r3, 1               # return true
//   0x822A6D9C  stfs   f0(0.0), 0x13C(r29) # mfHiddenBoost = 0.0f
//   0x822A6DA0  stb    r27(0), 0x140(r29)  # mbBoostInterrupted = false
//   0x822A6DA4  stw    r27(0), 0x144(r29)  # miChainSize = 0
//   0x822A6DB4  lfs    f31, flt_82001CC0@l # 0.0f
//   0x822A6DB8  fcmpu  cr6, f0, f31
//   0x822A6DBC  bne    cr6, loc_822A6E34
//   0x822A6DC4  lfs    f12, 0x13C(r29)     # mfHiddenBoost
//   0x822A6DCC  lfs    f0,  flt_820147FC@l # 0.5f
//   0x822A6DD4  fmadds f1,  f13, f0, f12   # mfMaxBoost * 0.5f + mfHiddenBoost
//   0x822A6DD8  lwz    r11, 0xC4(r11)      # AddBoost
//   0x822A6DE0  bctrl
//   0x822A6DE4  lfs    f0,  0xA0(r29)      # mfBoostAmount (re-read AFTER AddBoost)
//   0x822A6DE8  lfs    f13, 0xA4(r29)      # mfMaxBoost
//   0x822A6DEC  fcmpu  cr6, f0, f13
//   0x822A6DF0  bne    cr6, loc_822A6E1C
//   0x822A6DF4  lwz    r11, 0x144(r29)     # miChainSize
//   0x822A6DFC  stfs   f31, 0x13C(r29)     # mfHiddenBoost = 0.0f
//   0x822A6E00  li     r3, 1               # return true
//   0x822A6E04  addi   r11, r11, 1
//   0x822A6E08  stb    r10(1), 0xCB(r29)   # mbChainNotifyPending = true
//   0x822A6E0C  stw    r11, 0x144(r29)     # ++miChainSize
//   0x822A6E1C  li     r11, 1
//   0x822A6E20  li     r3, 0               # return false
//   0x822A6E24  stb    r11, 0x135(r29)     # mbLeaveBlueDueToInsufficientHidden = true
//   0x822A6E34  lbz    r11, 0x140(r29)     # mbBoostInterrupted
//   0x822A6E38  cntlzw r11, r11
//   0x822A6E3C  extrwi r3,  r11, 1,26      # return (mbBoostInterrupted == 0)
//
// The blue bar is all-or-nothing: you may only START a blue boost from a full bar,
// and if the bar is empty the accumulated HIDDEN boost is cashed in for half a bar
// (AddBoost clamps at mfMaxBoost) -- if that lands the bar exactly at full the
// chain continues and is notified, otherwise there was not enough hidden boost and
// the strategy flags itself to drop back to red. Anything in between (a partly
// drained bar) simply continues the current run unless it was interrupted.
//
// DIVERGENCES FROM Feb-2007 (BrnBoostBurnout5.cpp:304-345) -- the asm wins:
//   * the retail function takes NO parameters. Feb-2007's `CarOffenceManager*
//     lpCarOffenceManager` is gone, and with it BOTH of its calls --
//     lpCarOffenceManager->OnResetBoostChain() in the full-bar arm and
//     ->OnBoostChain() in the chain arm. r3 is the only incoming register the
//     prologue keeps (mr r29,r3) and nothing else is read; there is no call in
//     either arm besides AddBoost.
//   * Feb-2007 orders the full-bar arm OnResetBoostChain / mbBoostInterrupted /
//     mfHiddenBoost / miChainSize; retail stores mfHiddenBoost first. Same effect,
//     asm order kept.
//   * the assert line moved 307 -> 349 (`li r5,0x15D`).
// The 0.5f is flt_820147FC (the same pooled literal SetOncomingState @0x822A6138
// uses to re-arm the oncoming fade); Feb-2007 spells it `( 0.5f * mfMaxBoost )`,
// and the fmadds operand order (mfMaxBoost * 0.5f + mfHiddenBoost) agrees.
//
// NaN polarity: both `==` tests come from fcmpu+bne, which branches to the ELSE arm
// on unordered -- exactly C++ `==` (false for NaN). No inversion needed. The final
// cntlzw/extrwi pair is the branchless `== 0` test on a bool, i.e. logical NOT.
//
// AddBoost is BoostStrategy's protected slot 49 and BoostBurnout5 does not override
// it, so the `lwz r11,0xC4(vptr)` dispatches to the base body @0x822C0E10 -- a plain
// call here compiles to the same virtual dispatch. (Feb-2007 routed most B5 earnings
// through a helper `AddBoostB5` that split blue-mode gains into mfHiddenBoost; that
// helper does not exist in retail -- it is absent from the DecFIGS DWARF method list
// and from the X360 ledger -- and this body called plain AddBoost even in Feb-2007.)
// ---------------------------------------------------------------------------
bool
BoostBurnout5::BlueModeRequestBoost()
{
    // BrnBoostBurnout5.cpp:349 on the console (the assert's line immediate is
    // 0x15D). Reported, then execution continues into the body below -- the
    // console streams the literal into CgsDev::Assert::gpcMessageBuffer and calls
    // FireAssert with the buffer; there is no early-out on either path.
    CGS_ASSERT(meBoostMode == E_BOOSTMODE_B2_BLUE,
               "Incorrect Boost Mode when using a Blue-Mode-specific function");

    if (mbLeaveBlueDueToCrash)
    {
        // Cash the hidden boost straight into the bar on the way out of blue.
        AddBoost(mfHiddenBoost);
        mfHiddenBoost = 0.0f;

        return false;
    }
    else if (mfBoostAmount == mfMaxBoost)
    {
        mfHiddenBoost = 0.0f;
        mbBoostInterrupted = false;
        miChainSize = 0;

        return true;
    }
    else if (mfBoostAmount == 0.0f)
    {
        AddBoost((0.5f * mfMaxBoost) + mfHiddenBoost);

        if (mfBoostAmount == mfMaxBoost)
        {
            mfHiddenBoost = 0.0f;
            mbChainNotifyPending = true;
            miChainSize++;

            return true;
        }
        else
        {
            mbLeaveBlueDueToInsufficientHidden = true;

            return false;
        }
    }

    return !mbBoostInterrupted;
}

} // namespace BrnWorld
