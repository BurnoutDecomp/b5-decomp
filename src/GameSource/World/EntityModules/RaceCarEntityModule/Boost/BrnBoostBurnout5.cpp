// GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Created 2026-09-15 by tools/work/fold_partfiles.py --create-parent (b5-decomp issue #20).
// This family had NO parent TU: its bodies lived in 8 wave partfile(s), each
// with its own hand-written mount line in tools/build/build_game_exe.bat. They are folded
// here in MOUNT ORDER; every partfile's own header comment block is kept verbatim above
// its bodies (the address annotations are the evidence trail). No body was edited.
//
// Folded, in mount order:
//     BoostBurnout5_wP_01.cpp
//     BoostBurnout5_wP_02.cpp
//     BoostBurnout5_wP_03.cpp
//     BoostBurnout5_wP_04.cpp
//     BoostBurnout5_wP_05.cpp
//     BoostBurnout5_wP_06.cpp
//     BoostBurnout5_wP_11.cpp
//     BoostBurnout5_wP_16.cpp

// the union of the BoostBurnout5_w*.cpp partfiles' #include lines, first occurrence wins, mount order (2026-09-15)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)
#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"                 // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"   // Attrib::StringToKey
#include "rw/core/stdc/stdc.h"                                                       // rw::core::stdc::ConvertI64ToA
#include "types.hpp"                                                                 // f32 / s64
#include "GameSource/GameState/BrnGameActions.h"                                // BrnGameState::GameStateModuleIO::CompletedStuntAction

// ============================================================================
// FOLDED FROM BoostBurnout5_wP_01.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

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

// ============================================================================
// FOLDED FROM BoostBurnout5_wP_02.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 02.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only).
//
// Functions in this partfile:
//   BoostBurnout5::IsBlueMode               @ 0x822A6C50   (vtable slot 17)
//   BoostBurnout5::GetName                  @ 0x822A6F88   (vtable slot 15)
//   BoostBurnout5::GetIsChainNotifyPending  @ 0x822A70C0   (vtable slot 18)
//
// Offset -> member map used below. The base layout is the wave-P keystone
// header BrnBoostStrategy.h; BoostBurnout5's own members start at +0x130
// because sizeof(BoostStrategy) == 0x130 on BOTH console and host, and every
// B5 member offset here is pinned by BoostBurnout5::Prepare @0x822C1D58:
//   +0x0CB BoostStrategy::mbChainNotifyPending   (base bool block, Prepare stb 0 -> +203)
//   +0x130 BoostBurnout5::meBoostMode            (Prepare stw 0 -> +304, DWARF h:149)
//   +0x144 BoostBurnout5::miChainSize            (Prepare stw 0 -> +324, DWARF h:165)
//
// These three bodies were compile-validated out of tree against a probe header
// carrying the full DWARF member list, which also proved the console offsets
// hold on the host (offsetof meBoostMode == 0x130, miChainSize == 0x144,
// sizeof(BoostBurnout5) == 0x1B0): scratchpad/waveP/probe5/ in the workflow repo.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// IsBlueMode @ 0x822A6C50  (vtable slot 17)
//
//   0x822A6C50  lwz    r11, 0x130(r3)      # meBoostMode
//   0x822A6C54  addi   r11, r11, -1
//   0x822A6C58  cntlzw r11, r11
//   0x822A6C5C  extrwi r3, r11, 1,26       # r3 = (meBoostMode - 1) == 0
//   0x822A6C60  blr
//
// The cntlzw/extrwi pair is the compiler's branchless "== 0" test on the
// decremented value, i.e. a strict equality against 1 -- NOT a `!= 0` test, so
// this is `meBoostMode == E_BOOSTMODE_B2_BLUE` and not `meBoostMode != RED`
// (indistinguishable on a 2-valued enum today, but the asm pins the equality).
// +0x130 is BoostBurnout5's FIRST member (sizeof(BoostStrategy) == 0x130), and
// Prepare @0x822C1D58 stores 0 there (E_BOOSTMODE_B3_RED) as the initial mode.
//
// Feb-2007 (BrnBoostBurnout5.cpp:145) agrees exactly, modulo the enumerator
// spelling: it writes `BoostMode_B2_Blue`; the DecFIGS DWARF (the near-ancestor
// of ARTIST) spells the enumerators E_BOOSTMODE_B3_RED / E_BOOSTMODE_B2_BLUE,
// which is also what the project naming convention requires.
//
// NOTE vs the base class: BoostStrategy::IsBlueMode (slot 17) is `return false`;
// BoostBurnout5 is the strategy that actually has a blue mode.
// ---------------------------------------------------------------------------
bool
BoostBurnout5::IsBlueMode()
{
    return meBoostMode == E_BOOSTMODE_B2_BLUE;
}

// ---------------------------------------------------------------------------
// GetName @ 0x822A6F88  (vtable slot 15; pure in the base)
//
//   0x822A6F88  lis    r11, aBurnout5Rules@ha
//   0x822A6F8C  addi   r3,  r11, aBurnout5Rules@l   # "Burnout 5 rules"
//   0x822A6F90  blr
//
// A single .rdata string address returned in r3; `this` is never touched, which
// is consistent with the DWARF's trailing `const`
// (`virtual const char * GetName() const;`, BrnBoostBurnout5.h). Feb-2007
// (BrnBoostBurnout5.cpp:642) is identical.
// ---------------------------------------------------------------------------
const char*
BoostBurnout5::GetName() const
{
    return "Burnout 5 rules";
}

// ---------------------------------------------------------------------------
// GetIsChainNotifyPending @ 0x822A70C0  (vtable slot 18; overrides the base
// @0x822A5DF0, which just asserts, writes 0 and returns false)
//
//   0x822A70D4  mr     r30, r4                 # lpOutNumChained
//   0x822A70D8  mr     r31, r3                 # this
//   0x822A70DC  cmplwi cr6, r30, 0
//   0x822A70E0  bne    cr6, loc_822A7104
//   0x822A70E4  bl     CgsDev__Assert__BeginAssert
//   0x822A70E8  lis    r11, aDP4B5MainBurno_532@ha
//   0x822A70EC  li     r5,  0x365              # line 869
//   0x822A70F0  addi   r4,  r11, ...@l         # "d:\p4\b5_main\...\BrnBoostBurnout5.cpp"
//   0x822A70F4  lis    r11, aLpoutnumchaine@ha
//   0x822A70F8  addi   r3,  r11, ...@l         # "lpOutNumChained"
//   0x822A70FC  bl     CgsDev__Assert__FireAssert
//   0x822A7100  bl     CgsDev__Assert__EndAssert
//   0x822A7104  lwz    r11, 0x144(r31)         # miChainSize
//   0x822A7108  li     r10, 0
//   0x822A710C  cmpwi  cr6, r11, 0             # SIGNED compare
//   0x822A7110  bgt    cr6, loc_822A711C
//   0x822A7114  stw    r10, 0(r30)             # *lpOutNumChained = 0
//   0x822A7118  b      loc_822A7120
//   0x822A711C  stw    r11, 0(r30)             # *lpOutNumChained = miChainSize
//   0x822A7120  lbz    r3,  0xCB(r31)          # return mbChainNotifyPending
//   0x822A7124  stb    r10, 0xCB(r31)          # ...and clear it (read-and-clear)
//   0x822A713C  blr
//
// Three things the asm pins:
//   * the assert does NOT early-out -- the Begin/Fire/End triple falls straight
//     through into the (unconditional) store through lpOutNumChained. CGS_ASSERT
//     is report-and-continue on this build, so the plain macro matches.
//   * miChainSize is compared SIGNED (`cmpwi`) and only a strictly positive
//     value is published; the out-parameter is uint32_t (DWARF
//     `virtual bool GetIsChainNotifyPending(uint32_t *)`), so the guard is what
//     stops a negative chain size becoming a huge unsigned count.
//   * mbChainNotifyPending is read at +0xCB and cleared in the same breath --
//     the same latch idiom as BoostStrategy::IsATrafficCheckPending @0x822A5E98
//     and HasJustLostBoostChunk @0x822A5EC0.
//
// DIVERGENCE FROM Feb-2007 (asm wins): Feb-2007 declares mbChainNotifyPending as
// a BoostBurnout5 member (BrnBoostBurnout5.h:161) and has no assert at all. In
// retail the flag lives in the BASE at +0xCB (< 0x130, i.e. inside
// BoostStrategy), and the null-pointer assert at BrnBoostBurnout5.cpp:869 was
// added. Everything else -- the signed >0 guard and the read-and-clear -- is
// unchanged from Feb-2007 (BrnBoostBurnout5.cpp:158-174).
// ---------------------------------------------------------------------------
bool
BoostBurnout5::GetIsChainNotifyPending(u32* lpOutNumChained)
{
    // BrnBoostBurnout5.cpp:869 on the console (the assert's line immediate is
    // 0x365); reported, then execution continues into the store below.
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
// FOLDED FROM BoostBurnout5_wP_03.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 03.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only -- and for ALL THREE of
// these functions retail contradicts it, see the per-body DIVERGENCE notes).
//
// Functions in this partfile:
//   BoostBurnout5::OnCrash               @ 0x822A6F78   (vtable slot 7)
//   BoostBurnout5::OnDriveThru           @ 0x822C1FC0   (vtable slot 46)
//   BoostBurnout5::OnEnterInfiniteBoost  @ 0x822C23E0   (vtable slot 2)
//
// Offset -> member map used below. The base layout is the wave-P keystone
// header BrnBoostStrategy.h; BoostBurnout5's own members start at +0x130
// because sizeof(BoostStrategy) == 0x130 on BOTH console and host.
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0C5 BoostStrategy::mbBoosting
//   +0x0E8 BoostStrategy::miCombinedBoostLevel   (s32)
//   +0x104 BoostStrategy::miBoostLevel           (s32)
//   +0x130 BoostBurnout5::meBoostMode            (DWARF h:149; Prepare @0x822C1D58 stw 0)
//   +0x134 BoostBurnout5::mbLeaveBlueDueToCrash             (DWARF h:151)
//   +0x135 BoostBurnout5::mbLeaveBlueDueToInsufficientHidden (DWARF h:152)
// The DWARF member order (meBoostMode, then the four bools, then mbAllowBoostEarning,
// mfHiddenBoost @+0x13C) is corroborated on X360 by RemoveAllBoostAndChunks
// @0x822C2410, which stores 0.0f -> +0x13C (mfHiddenBoost) and 0 -> +0x130.
//
// These three bodies were compile-validated out of tree against a probe header
// carrying the DWARF member list; the probe's in-class static_asserts confirmed
// the console offsets hold unchanged on the x64 host (offsetof meBoostMode ==
// 0x130, mbLeaveBlueDueToCrash == 0x134, mbLeaveBlueDueToInsufficientHidden ==
// 0x135, mfHiddenBoost == 0x13C). Probe: scratchpad/waveP/probe5_wP03/ in the
// workflow repo (STATUS=pass).
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnCrash @ 0x822A6F78  (vtable slot 7; pure in the base)
//
//   0x822A6F78  li   r11, 0
//   0x822A6F7C  stb  r11, 0xC5(r3)      # mbBoosting = false
//   0x822A6F80  blr
//   0x822A6F84  .long 0                 # alignment padding; GetName starts at 0x822A6F88
//
// The whole retail body is that one store. There is no fall-through: the blr is
// followed by padding and then a different function, so nothing is missing.
//
// The store is corroborated as a shared idiom rather than a mis-decode: the
// sibling strategies' OnCrash bodies BOTH end with the identical
// `li r11,0 / stb r11,0xC5(r3)` -- BoostBurnout2::OnCrash @0x822A63A0 and
// BoostBurnout3::OnCrash @0x822A6918 -- so retail's OnCrash contract includes
// "stop boosting". (This is plausibly the inlined body of the base's
// header-inline BoostStrategy::TurnOffBoosting (DWARF BrnBoostStrategy.h:439),
// which has no standalone X360 symbol and no Feb-2007 counterpart at all. That
// helper's body is NOT recovered, so this is written as the attested member
// store; do not "restore" a call to it without an anchor.)
//
// DIVERGENCE FROM Feb-2007 (BrnBoostBurnout5.cpp:508): the leaked body is a
// three-way affair --
//     if( meBoostMode == BoostMode_B3_Red ) { RemoveBoostB5( mfOnCrashBoostToLoseProportion
//         * mfBoostAmount ); if( miBoostLevel > 0 ) miBoostLevel--; }
//     else if( meBoostMode == BoostMode_B2_Blue ) mbLeaveBlueDueToCrash = true;
//     UpdateMaxBoost();
// None of that survives in retail: there is no mode test, no boost removal, no
// miBoostLevel decrement, no mbLeaveBlueDueToCrash store and no UpdateMaxBoost
// call at this address. Retail routes B5's crash handling through the base
// SetCrashing/mbCrashing state instead (BoostStrategy::SetCrashing @0x822A5F00
// fires this on the false->true edge and stores mbCrashing, which AddBoost
// @0x822C0E10 then gates on). The asm wins.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnCrash()
{
    mbBoosting = false;
}

// ---------------------------------------------------------------------------
// OnDriveThru @ 0x822C1FC0  (vtable slot 46; pure in the base)
//
//   0x822C1FC0  li  r4, 1
//   0x822C1FC4  b   BrnWorld::BoostStrategy::UpdateMaxBoost      # tail call
//
// Two instructions: set the single bool argument and tail-branch. r3 is passed
// straight through as `this`, r4 = 1 is UpdateMaxBoost's `lbFillToMax`, so the
// drive-thru refills the bar to whatever the current boost level allows
// (UpdateMaxBoost @0x822C0EB0 recomputes mfMaxBoost from miCombinedBoostLevel
// and then, because lbFillToMax is set, assigns mfBoostAmount = mfMaxBoost).
//
// The branch target is the BASE's UpdateMaxBoost, which is NON-virtual (a plain
// `b` to a fixed address, not a vtable dispatch through r3's vptr) -- unlike
// BoostBurnout3, which declares its OWN `virtual void UpdateMaxBoost(bool)` and
// whose bodies dispatch +0xC8. BoostBurnout5 declares no such override (DWARF
// BrnBoostBurnout5.h has no UpdateMaxBoost), so the unqualified call below
// resolves to BoostStrategy::UpdateMaxBoost exactly as the asm does.
//
// DIVERGENCE FROM Feb-2007: BoostBurnout5::OnDriveThru does not exist anywhere
// in the leaked drop -- neither BrnBoostBurnout5.h nor .cpp mentions it, and
// the base has no OnDriveThru either. It is a post-Feb-2007 addition; the
// DecFIGS DWARF has it at BrnBoostBurnout5.cpp:199 and the base declares it
// pure (slot 46), so there is no reference body to reconcile against. Shape
// (void return, no parameters) is the DWARF's.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnDriveThru()
{
    UpdateMaxBoost(true);
}

// ---------------------------------------------------------------------------
// OnEnterInfiniteBoost @ 0x822C23E0  (vtable slot 2; pure in the base)
//
//   0x822C23E0  lwz   r10, 0xE8(r3)            # miCombinedBoostLevel
//   0x822C23E4  lis   r9,  flt_82014808@ha
//   0x822C23E8  li    r11, 0
//   0x822C23EC  addi  r10, r10, -1             # miCombinedBoostLevel - 1
//   0x822C23F0  li    r4,  0                   # UpdateMaxBoost's lbFillToMax = false
//   0x822C23F4  lfs   f0,  flt_82014808@l(r9)  # 100.0f
//   0x822C23F8  stfs  f0,  0xA0(r3)            # mfBoostAmount = 100.0f
//   0x822C23FC  stw   r11, 0x130(r3)           # meBoostMode = E_BOOSTMODE_B3_RED (0)
//   0x822C2400  stb   r11, 0x134(r3)           # mbLeaveBlueDueToCrash = false
//   0x822C2404  stw   r10, 0x104(r3)           # miBoostLevel = miCombinedBoostLevel - 1
//   0x822C2408  stb   r11, 0x135(r3)           # mbLeaveBlueDueToInsufficientHidden = false
//   0x822C240C  b     BrnWorld::BoostStrategy::UpdateMaxBoost   # tail call, r4 = 0
//
// Interleaving is scheduling only -- the six stores are independent, so the
// source order below is the Feb-2007 order (mode, level, amount, the two blue
// flags, UpdateMaxBoost) rather than the emitted order.
//
// 100.0f is flt_82014808, the SAME merged .rdata slot BoostStrategy::Prepare
// @0x822A5C60 loads for `mfMaxBoost = 100.0f` and BoostBurnout3::
// OnEnterInfiniteBoost @0x822A6A8C loads for its own mfBoostAmount store.
// Written as a literal, not as the base's `KF_MAX_MAX_BOOST` (which is what
// Feb-2007's member `mfMaxMaxBoost` became): constant merging makes a named
// constant and a literal indistinguishable in the image, and there is no
// independent anchor -- same call the BoostBurnout3 partfile made, kept
// consistent across the wave. Note that the value overshoots on purpose:
// UpdateMaxBoost(false) immediately clamps mfBoostAmount into [0, mfMaxBoost].
//
// DIVERGENCE FROM Feb-2007 (BrnBoostBurnout5.cpp:682): the leaked body seeds
// the red-mode bar from a COMPILE-TIME constant --
//     miBoostLevel = KI_BOOST_LEVELS_IF_RED_MODE - 1;   // == 4 (DWARF h:160 == 5)
// whereas retail reads miCombinedBoostLevel (+0xE8, the base's car-stat boost
// level written by SetCarStatBoostLevel @0x822D52AC) and decrements THAT. So
// the number of red-mode segments became car-dependent between Feb-2007 and
// ship; KI_BOOST_LEVELS_IF_RED_MODE is not referenced by this body at all.
// (+0xE8 is a base member, not an embedded sub-object: the base's own span runs
// to +0x12F, so this read is inside BoostStrategy, not BoostBurnout5.)
// Feb-2007 also spells the amount `mfMaxMaxBoost` (a member loaded from the
// attrib record, `lBoostParams.Max()`), which retail no longer has -- the base
// class has no such member and Prepare stores the same 100.0f literal.
// The rest of the Feb-2007 body (mode = red, both blue-exit flags cleared,
// trailing UpdateMaxBoost) survives verbatim.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnEnterInfiniteBoost()
{
    meBoostMode  = E_BOOSTMODE_B3_RED;
    miBoostLevel = miCombinedBoostLevel - 1;

    mfBoostAmount = 100.0f;

    mbLeaveBlueDueToCrash              = false;
    mbLeaveBlueDueToInsufficientHidden = false;

    UpdateMaxBoost(false);
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout5_wP_04.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 04 (three event hooks).
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
//   0x822A6E50  BoostBurnout5::OnShortcut         (vtable slot 11)
//   0x822A6A20  BoostBurnout5::OnSlammed          (vtable slot  9)
//   0x822D5330  BoostBurnout5::OnStartCrashPlay   (vtable slot 13)
//
// SOURCES (authority order): X360 ARTIST asm at the three addresses above is
// rung 1 and arbitrates every branch, constant and store; the DecFIGS DWARF
// BrnBoostBurnout5.h gives declaration shape; Feb-2007 BrnBoostBurnout5.cpp is
// idiom only -- all three bodies diverge from it, see the notes per body.
//
// VTABLE / CALL DECODE (X360, 4-byte slots):
//   +0xC4 == slot 49 == BoostStrategy::AddBoost(f32)  -- protected, virtual.
//            BoostBurnout5 does NOT override it (no AddBoost in its DWARF
//            virtual list), so an unqualified call from a B5 body is a virtual
//            dispatch through the base slot, exactly as the asm shows.
//   BoostStrategy::UpdateMaxBoost(bool) is NON-virtual and BoostBurnout5 does
//            not redeclare it, so OnStartCrashPlay reaches it with a DIRECT
//            branch (`b BrnWorld__BoostStrategy__UpdateMaxBoost`) -- unlike
//            BoostBurnout3, which declares its OWN virtual UpdateMaxBoost and
//            therefore dispatches through its extension slot +0xC8.
//   BoostStrategy::RemoveBoost(f32) is NON-virtual and has no standalone X360
//            symbol -- it is inlined into every caller (this is why OnSlammed
//            is five instructions with no call).
//
// MEMBER DECODE (all base members; console offsets == host offsets, keystone):
//   +0x07C BoostStrategy::mfBeingSlammed          (attrib param rec+0x78)
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0E8 BoostStrategy::miCombinedBoostLevel
//   +0x100 BoostStrategy::mfMinBoostAllowedAmount
//   +0x104 BoostStrategy::miBoostLevel
//
// .RDATA CONSTANTS USED (values pinned from the image, not from memory):
//   flt_8201499C == 30.0f          -- also the KF_MIN_TIME_FOR_BOOST_TRAINING_TIP
//        threshold in BoostStrategy::Update @0x822F8228 and the literal 30.0
//        Hex-Rays prints in RaceCarEntityModuleDebugComponent::RenderEngineState
//        @0x822C3080 and RaceCarEntityModule::DebugRenderPosition @0x822BDAB0.
//        Four independent renderings agree on 30.0f.
//   flt_82014460 == 1.1920929e-07f == 0x34000000 == FLT_EPSILON -- also the
//        near-zero spin-angle threshold in BoostStrategy::Update @0x822F82CC and
//        the same value BoostBurnout3::OnStartCrashPlay @0x822A6AD8 stores into
//        mfMinBoostAllowedAmount.
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnShortcut @ 0x822A6E50  (vtable slot 11; pure in the base)
//
//   0x822A6E50  lis    r11, flt_8201499C@ha
//   0x822A6E54  lwz    r10, 0(r3)                 # vptr
//   0x822A6E58  lfs    f1,  flt_8201499C@l(r11)   # 30.0f  (NOT a member load)
//   0x822A6E5C  lwz    r11, 0xC4(r10)             # slot 49 = AddBoost
//   0x822A6E60  mtctr  r11
//   0x822A6E64  bctr                              # tail call, no frame
//
// The reward is loaded from .rdata with an ABSOLUTE address, not from `this` --
// so unlike OnTrafficCheck/OnTakedown (which retail retuned onto the shared
// boostparamsasset params mfTrafficCheck@+0x48 / mfTakedownEarning@+0x28) the
// shortcut reward stayed a class-static constant. There is no "Shortcut"
// attribute in the 34-entry boostparamsasset record, which is consistent.
// DWARF names that static BoostStrategy::KF_ON_SHORTCUT (BrnBoostStrategy.h:375,
// the retail rename of Feb-2007's BoostStrategy::mfOnShortcutBoostReward);
// its DEFINITION belongs to the BrnBoostStrategy.cpp wave, and this body pins
// its value: KF_ON_SHORTCUT == 30.0f.
//
// FEB-2007 DIVERGENCE: Feb-2007 (BrnBoostBurnout5.cpp:433) is
// `AddBoostB5( mfOnShortcutBoostReward )`. Retail has no AddBoostB5 at all (it
// is absent from the B5 DWARF), so the earning goes through the base virtual
// AddBoost and none of AddBoostB5's meBoostMode / mbInfiniteBoost gating
// survives -- the asm loads nothing but the vptr and the constant.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnShortcut()
{
    AddBoost( KF_ON_SHORTCUT );          // flt_8201499C == 30.0f
}


// ---------------------------------------------------------------------------
// OnSlammed @ 0x822A6A20  (vtable slot 9; pure in the base)
//
//   0x822A6A20  lfs    f0,  0xA0(r3)              # mfBoostAmount
//   0x822A6A24  lis    r11, flt_82001CC0@ha
//   0x822A6A28  lfs    f13, 0x7C(r3)              # mfBeingSlammed
//   0x822A6A2C  fsubs  f0, f0, f13
//   0x822A6A30  stfs   f0,  0xA0(r3)              # mfBoostAmount -= param
//   0x822A6A34  lfs    f13, flt_82001CC0@l(r11)   # 0.0f
//   0x822A6A38  fcmpu  cr6, f0, f13
//   0x822A6A3C  bgelr  cr6                        # >= 0 -> done
//   0x822A6A40  stfs   f13, 0xA0(r3)              # else clamp to 0
//   0x822A6A44  blr
//
// That is BoostStrategy::RemoveBoost(f32) inlined verbatim (it is non-virtual
// and has no standalone X360 symbol; DecFIGS body @BrnBoostStrategy.cpp:168 and
// Feb-2007 BrnBoostStrategy.h:469 both read
// `mfBoostAmount -= lfBoostAmount; if( mfBoostAmount < 0.0f ) mfBoostAmount = 0.0f;`),
// so the call is restored here per the de-optimization rule.
//
// NaN POLARITY CHECK: `fcmpu` + `bgelr` is `bc 4,LT` -- LT is clear when the
// compare is unordered, so a NaN result TAKES the branch and leaves the stored
// NaN in place. C++ `if( x < 0.0f )` is likewise false for NaN, so the plain
// `<` form (not a negated `>=`) is the faithful transliteration here.
//
// FEB-2007 DIVERGENCE (two): Feb-2007 (BrnBoostBurnout5.cpp:551) is
// `RemoveBoostB5( mfSlamLoss )`.
//   (a) RemoveBoostB5 -- which early-outs on mbInfiniteBoost and branches on
//       meBoostMode (setting mbBoostInterrupted in BLUE, asserting on an
//       unsupported mode) -- does not exist in retail: the asm reads neither
//       mbInfiniteBoost nor meBoostMode(+0x130), and there is no branch at all.
//       It is the plain base RemoveBoost.
//   (b) The amount is the boostparamsasset tuning param mfBeingSlammed (+0x7C,
//       attrib "BeingSlammed" rec+0x78), NOT the class-static mfSlamLoss
//       (DWARF KF_SLAM_LOSS) -- the asm loads it off `this`, not off .rdata.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnSlammed()
{
    RemoveBoost( mfBeingSlammed );
}


// ---------------------------------------------------------------------------
// OnStartCrashPlay @ 0x822D5330  (vtable slot 13; NOT pure in the base)
//
//   0x822D5330  lwz    r11, 0xE8(r3)              # miCombinedBoostLevel
//   0x822D5334  lis    r10, flt_82014460@ha
//   0x822D5338  li     r4,  0                     # UpdateMaxBoost arg = false
//   0x822D533C  addi   r11, r11, -1
//   0x822D5340  lfs    f0,  flt_82014460@l(r10)   # FLT_EPSILON
//   0x822D5344  stfs   f0,  0x100(r3)             # mfMinBoostAllowedAmount
//   0x822D5348  stw    r11, 0x104(r3)             # miBoostLevel
//   0x822D534C  b      BrnWorld__BoostStrategy__UpdateMaxBoost   # tail call
//
// The `lwz 0xE8` is hoisted by the scheduler; the two stores retire in the
// order written below. UpdateMaxBoost recomputes mfMaxBoost from
// miCombinedBoostLevel and re-clamps mfBoostAmount, and its `false` argument
// means "do not fill to max". Effect: crash play drops the bar one segment
// below the car's combined level and removes the floor on the allowed amount.
//
// FEB-2007 DIVERGENCES (three): Feb-2007 (BrnBoostBurnout5.cpp:701) is
// `miBoostLevel = KI_BOOST_LEVELS_IF_RED_MODE - 1; UpdateMaxBoost();`.
//   (a) Retail derives the level from the RUNTIME member miCombinedBoostLevel
//       (+0xE8, written by SetCarStatBoostLevel @0x822D5290), not from the
//       compile-time KI_BOOST_LEVELS_IF_RED_MODE (== 5, DWARF B5 h:160): the
//       asm loads +0xE8 and does `addi -1`, it does not materialise a 4.
//   (b) The mfMinBoostAllowedAmount = FLT_EPSILON store is new (retail
//       BoostBurnout3::OnStartCrashPlay @0x822A6AD8 gained the same store).
//   (c) UpdateMaxBoost gained the bool parameter, passed false here.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnStartCrashPlay()
{
    mfMinBoostAllowedAmount = 1.1920929e-07f;   // flt_82014460 == FLT_EPSILON
    miBoostLevel            = miCombinedBoostLevel - 1;

    UpdateMaxBoost( false );
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout5_wP_05.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 05.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only -- and for all three of
// these bodies retail CONTRADICTS Feb-2007, see the per-function notes).
//
// Functions in this partfile:
//   BoostBurnout5::OnStuntCompletion  @ 0x822A6C68   (vtable slot 10)
//   BoostBurnout5::OnTrafficCheck     @ 0x822A6F18   (vtable slot 12)
//   BoostBurnout5::OnTakedown         @ 0x822A6F60   (vtable slot 3)
//
// All three are one-line "earn a tuning-table amount" handlers: each loads a
// f32 out of the BoostStrategy tuning block and dispatches vtable slot 49
// (+0xC4 == 49 * 4) -- BoostStrategy::AddBoost, pinned as slot 49 by the
// keystone (OnModeStart @0x822A6030 dispatches the same +0xC4). BoostBurnout5
// does NOT override AddBoost (no such DWARF declaration, no X360 identity), so
// the dispatch lands on the base body @0x822C0E10; it is emitted as a virtual
// call because AddBoost is a non-final virtual, which is exactly what plain
// `AddBoost(x)` compiles to.
//
// Offset -> member map used below (base layout from the wave-P keystone header
// BrnBoostStrategy.h; every one of these is a BoostStrategy member, this
// partfile touches NO BoostBurnout5-own member):
//   +0x028 BoostStrategy::mfTakedownEarning       (attrib rec+0x08 TakedownEarning)
//   +0x048 BoostStrategy::mfTrafficCheck          (attrib rec+0x00 TrafficCheck)
//   +0x080 BoostStrategy::mfStuntJumpEarning      (attrib rec+0x14 StuntJumpEarning)
//   +0x088 BoostStrategy::mfStuntBillBoardEarning (attrib rec+0x18 StuntBillBoardEarning)
//   +0x0C2 BoostStrategy::mbJustTrafficChecked    (IsATrafficCheckPending @0x822A5E98)
//
// Compile-validated out of tree (b5-decomp/.../BrnBoostBurnout5.h does not
// exist yet and this pass may not create headers) against the scratchpad probe
// header carrying the full DWARF member list:
// scratchpad/waveP/probe5g05/ in the workflow repo.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnStuntCompletion @ 0x822A6C68  (vtable slot 10; pure in the base)
//
//   0x822A6C68  cmpwi  cr6, r4, 0
//   0x822A6C6C  beq    cr6, loc_822A6C8C     # element == JUMP
//   0x822A6C70  cmpwi  cr6, r4, 2
//   0x822A6C74  bnelr  cr6                   # element != BILLBOARD -> return
//   0x822A6C78  lwz    r11, 0(r3)            # vptr
//   0x822A6C7C  lfs    f1,  0x88(r3)         # mfStuntBillBoardEarning
//   0x822A6C80  lwz    r11, 0xC4(r11)        # slot 49 = AddBoost
//   0x822A6C84  mtctr  r11
//   0x822A6C88  bctr                         # tail call
//   loc_822A6C8C:
//   0x822A6C8C  lwz    r11, 0(r3)
//   0x822A6C90  lfs    f1,  0x80(r3)         # mfStuntJumpEarning
//   0x822A6C94  lwz    r11, 0xC4(r11)
//   0x822A6C98  mtctr  r11
//   0x822A6C9C  bctr                         # tail call
//
// r4 is the element type (an int-width enum; no FPR is consumed before it, so
// no PPC float-arg GPR skip is in play here). DWARF pins the parameter type:
// `virtual void OnStuntCompletion(BrnGameState::StuntElementType)`
// (BrnBoostBurnout5.h, DWARF body @cpp:251), and the enumerators are
// JUMP = 0 / SMASH = 1 / BILLBOARD = 2 (BrnGameStateTypes.h). The two tested
// values 0 and 2 therefore map exactly onto the two tuning params whose names
// match -- StuntJumpEarning and StuntBillBoardEarning -- which is the
// independent confirmation that this really is the element-type enum.
//
// SMASH (1) falls through and earns NOTHING in retail, even though
// mfStuntSmashEarning (+0x84) exists in the tuning block: the asm has no third
// arm and no load of +0x84 anywhere in this body. (Consistent with
// BoostBurnout5::Prepare @0x822C1F8C-94, which re-zeroes all three Stunt*
// earning params right after the attrib load -- B5 pays stunt boost through
// UpdateStuntBoost instead.)
//
// FEB-2007 DIVERGENCE: the Feb-2007 body (BrnBoostBurnout5.cpp:226) takes NO
// parameter at all and unconditionally does `AddBoostB5(mfBoostAmountOnStunt)`
// -- a per-B5 static const and a B5-private AddBoostB5 helper. Retail has
// neither: the signature grew the element-type argument, the reward moved into
// the shared boostparamsasset tuning block, and the earn goes through the base
// virtual AddBoost. The asm wins.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnStuntCompletion(
    BrnGameState::StuntElementType leElementType )
{
    switch( leElementType )
    {
    case BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP:
        AddBoost( mfStuntJumpEarning );
        break;

    case BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD:
        AddBoost( mfStuntBillBoardEarning );
        break;

    default:
        // E_STUNT_ELEMENT_TYPE_SMASH earns nothing in retail (no arm in the asm).
        break;
    }
}

// ---------------------------------------------------------------------------
// OnTrafficCheck @ 0x822A6F18  (vtable slot 12; pure in the base)
//
//   0x822A6F18  mflr   r12
//   0x822A6F1C  stw    r12, var_8(r1)
//   0x822A6F20  std    r31, var_10(r1)
//   0x822A6F24  stwu   r1, back_chain(r1)
//   0x822A6F28  mr     r31, r3               # this
//   0x822A6F2C  lwz    r11, 0(r31)           # vptr
//   0x822A6F30  lfs    f1,  0x48(r31)        # mfTrafficCheck
//   0x822A6F34  lwz    r11, 0xC4(r11)        # slot 49 = AddBoost
//   0x822A6F38  mtctr  r11
//   0x822A6F3C  bctrl                        # CALL (not a tail call)
//   0x822A6F40  li     r11, 1
//   0x822A6F44  stb    r11, 0xC2(r31)        # mbJustTrafficChecked = true
//   0x822A6F48..58  epilogue
//
// The store happens AFTER the AddBoost call returns -- that is why this body
// (unlike OnTakedown) sets up a frame and uses bctrl rather than bctr.
//
// FEB-2007 DIVERGENCE: Feb-2007 (BrnBoostBurnout5.cpp:456) is
// `AddBoostB5( mfOnTrafficCheckBoostReward )` -- a B5-private helper and a
// member that no longer exists. Retail earns the shared tuning param
// mfTrafficCheck through the base virtual AddBoost, and the
// mbJustTrafficChecked latch (read by BoostStrategy::IsATrafficCheckPending
// @0x822A5E98, slot 23) is NEW: Feb-2007 does not set it here.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnTrafficCheck()
{
    AddBoost( mfTrafficCheck );

    mbJustTrafficChecked = true;
}

// ---------------------------------------------------------------------------
// OnTakedown @ 0x822A6F60  (vtable slot 3; pure in the base)
//
//   0x822A6F60  lwz    r11, 0(r3)            # vptr
//   0x822A6F64  lfs    f1,  0x28(r3)         # mfTakedownEarning
//   0x822A6F68  lwz    r11, 0xC4(r11)        # slot 49 = AddBoost
//   0x822A6F6C  mtctr  r11
//   0x822A6F70  bctr                         # tail call, no frame
//
// The whole body: five instructions, no frame, no branch. Nothing is read or
// written besides the tuning param and the dispatch.
//
// FEB-2007 DIVERGENCE (the largest of the three): Feb-2007
// (BrnBoostBurnout5.cpp:470) switches on meBoostMode, and in RED mode bumps
// miBoostLevel up to KI_BOOST_LEVELS_IF_RED_MODE - 1, calls UpdateMaxBoost(),
// then AddBoostB5( mfMaxMaxBoost ); in BLUE mode AddBoostB5( mfMaxMaxBoost );
// otherwise asserts "Unsuported boost mode". Retail has NONE of that -- no
// load of meBoostMode (+0x130), no miBoostLevel/UpdateMaxBoost, no assert --
// just one virtual AddBoost of the shared tuning param mfTakedownEarning.
// The asm wins.
// ---------------------------------------------------------------------------
void
BoostBurnout5::OnTakedown()
{
    AddBoost( mfTakedownEarning );
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout5_wP_06.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 06.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom/naming only).
//
// Functions ASSIGNED to this partfile:
//   BoostBurnout5::Prepare                  @ 0x822C1D58   (vtable slot 0)  -- PARKED
//   BoostBurnout5::RemoveAllBoostAndChunks  @ 0x822C2410   (vtable slot 41) -- below
//   BoostBurnout5::UpdateStuntBoost         @ 0x822A6F98   (vtable slot 48) -- PARKED
//
// The two parked bodies are COMPLETE and asm-walked but need declarations that
// do not exist in b5-decomp/src and that this pass may not create. They live,
// with the verbatim unblock text, at (workflow repo, not this tree):
//   scratchpad/waveP/parked/BoostBurnout5_wP_6.parked.cpp
//     * Prepare          -- Attrib::Gen::boostparamsasset exposes no attribute
//                           accessors (it derives Attrib::Instance PRIVATELY and
//                           re-exports nothing), and rw::core::stdc::ConvertI64ToA
//                           is not declared anywhere in the tree.
//     * UpdateStuntBoost -- BrnGameState::GameStateModuleIO::CompletedStuntAction
//                           is an incomplete type everywhere in b5-decomp/src;
//                           the body dereferences four of its fields.
//
// SHARED WAVE DEPENDENCY (blocks this partfile and every other BoostBurnout5 /
// BoostBurnout2 / BoostBurnout3 partfile landed this wave): the class header
//   .../RaceCarEntityModule/Boost/BrnBoostBurnout5.h
// does not exist yet, so `selfcheck` on this file reports C1083 on line 1. The
// class is `BoostBurnout5 : public BoostStrategy`, adds NO virtuals to the base
// 50 slots, and its own members -- every offset pinned by Prepare @0x822C1D58 --
// are, in DecFIGS DWARF declaration order:
//   +0x130 BoostMode meBoostMode                      (stw  0 -> 304)
//   +0x134 bool      mbLeaveBlueDueToCrash            (stb  0 -> 308)
//   +0x135 bool      mbLeaveBlueDueToInsufficientHidden
//   +0x136 bool      mbSwicthBlueToRed                (sic -- DWARF spelling)
//   +0x137 bool      mbAddRemoveChunkMode
//   +0x138 bool      mbAllowBoostEarning
//   +0x13C f32       mfHiddenBoost                    (stfs 0.0f -> 316)
//   +0x140 bool      mbBoostInterrupted
//   +0x144 s32       miChainSize
//   +0x148 f32       mfBlueCrashDecrease              (stfs 50.0f -> 328)
//   +0x150 Vector3   mStuntRollInProgress             (stvx128 zero)
//   +0x160 Vector3   mvPositionLastFrame              (stvx128 zero)
//   +0x170 Vector3   mvTakeOffAtVec                   (stvx128 zero)
//   +0x180 Vector3   mvLandAtVec                      (stvx128 zero)
//   +0x190 f32       mfBearingLastFrame
//   +0x194 f32       mfAngleSoFar
//   +0x198 f32       mfTimeElapsed
//   +0x19C f32       mfTimeBoosting
//   +0x1A0 bool      mbHandbreakTurnAttempting
//   +0x1A1 bool      mbWasJustInTheAir
//   +0x1A2 bool      mbTestForCleanLanding
//   sizeof 0x1B0, align 16.
// plus `enum BoostMode { E_BOOSTMODE_B3_RED = 0, E_BOOSTMODE_B2_BLUE = 1 };`
// (DWARF BrnBoostBurnout5.h:143). Base offsets come from the wave-P keystone
// header BrnBoostStrategy.h; sizeof(BoostStrategy) == 0x130 on BOTH console and
// host, which is why the console offsets above are directly assertable here.
//
// Base members touched by the body below (BrnBoostStrategy.h):
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x104 BoostStrategy::miBoostLevel
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// RemoveAllBoostAndChunks @ 0x822C2410  (vtable slot 41; pure in the base)
//
//   0x822C2410  lis    r11, flt_82001CC0@ha
//   0x822C2414  li     r10, 0
//   0x822C2418  li     r4,  0                  # UpdateMaxBoost's lbFillToMax
//   0x822C241C  lfs    f0,  flt_82001CC0@l(r11)   # 0.0f
//   0x822C2420  stfs   f0,  0xA0(r3)           # mfBoostAmount   = 0.0f
//   0x822C2424  stw    r10, 0x104(r3)          # miBoostLevel    = 0
//   0x822C2428  stfs   f0,  0x13C(r3)          # mfHiddenBoost   = 0.0f
//   0x822C242C  stw    r10, 0x130(r3)          # meBoostMode     = E_BOOSTMODE_B3_RED
//   0x822C2430  b      BrnWorld__BoostStrategy__UpdateMaxBoost
//
// Four stores and a TAIL CALL. Three details the asm pins:
//   * the branch to UpdateMaxBoost is a plain `b` to the base symbol, not a
//     vtable dispatch -- BoostStrategy::UpdateMaxBoost is NON-virtual (the
//     `virtual void UpdateMaxBoost(bool)` that DecFIGS shows on BoostBurnout3
//     is B3's OWN new virtual, which hides rather than overrides, and does not
//     exist on BoostBurnout5 at all).
//   * r4 is set to 0 BEFORE the branch, so the tail call is UpdateMaxBoost
//     (false) -- i.e. clamp mfBoostAmount into the recomputed [0, mfMaxBoost],
//     do NOT refill to max. That is the whole point here: the bar was just
//     emptied and must stay empty.
//   * meBoostMode is reset to the RED (Burnout-3 rules) mode, which is also the
//     value Prepare @0x822C1DC0 seeds -- blue mode cannot survive having its
//     hidden reservoir taken away.
//
// Feb-2007 (BrnBoostBurnout5.cpp:729-737) is the same four assignments in a
// different order followed by `UpdateMaxBoost();`. DIVERGENCE (asm wins, but it
// is cosmetic): retail's UpdateMaxBoost takes an explicit lbFillToMax argument
// that the Feb-2007 signature did not have; the retail call passes false, which
// is the behaviour the Feb-2007 no-arg form had.
// ---------------------------------------------------------------------------
void
BoostBurnout5::RemoveAllBoostAndChunks()
{
    mfBoostAmount = 0.0f;
    miBoostLevel  = 0;
    mfHiddenBoost = 0.0f;
    meBoostMode   = E_BOOSTMODE_B3_RED;

    UpdateMaxBoost(false);
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout5_wP_11.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 11.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Functions in this partfile:
//   BoostBurnout5::ApplyUpdate  @ 0x822C1FC8   (vtable slot 1)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape and the GUID constant; Feb-2007 is idiom only --
// and it has drifted badly here, see the DIVERGENCES list on the function).
//
// Every offset quoted below was re-derived for this file from the 0x822C1FC8
// listing and cross-checked against the member map in BrnBoostStrategy.h /
// BrnBoostBurnout5.h. Console vtable slots are 4 bytes, so the `lwz 0xNN(vptr)`
// displacements divide by 4 to give the slot numbers the two headers enumerate.
// ============================================================================

namespace BrnWorld
{

// The boostparams asset GUID this strategy resolves its tuning collection from.
// NAME, TYPE and VALUE are all DecFIGS-attested verbatim -- the DWARF dump of
// BrnBoostBurnout5.cpp:46 declares
//   const int64_t KI_DEFAULT_STUNT_BOOST_PARAMS_GUID = 576013;
// and the X360 stages exactly that value inline at 0x822C225C/0x822C2268
// (`lis r3,8` + `ori r3,r3,0xCA0D` == 0x0008CA0D == 576013). int64_t is also
// what ConvertI64ToA takes. BoostBurnout2 uses 576005 and BoostBurnout3 576011;
// each strategy has its own collection. Same spelling and placement as the
// sibling BoostBurnout3_wP_16.cpp:48.
const s64 KI_DEFAULT_STUNT_BOOST_PARAMS_GUID = 576013;

// ---------------------------------------------------------------------------
// ApplyUpdate @ 0x822C1FC8  (vtable slot 1; PURE in the base. Its caller is the
// non-virtual BoostStrategy::Update @0x822F8130, which passes ONLY the time step
// -- the retail signature has no second parameter.)
//
// -- earning (0x822C1FE4-0x822C2108) ---------------------------------------
// Five identical `lwz vptr / lwz 0xNN(vptr) / bctrl` state queries, each gating
// `lfs <param> / fmuls f1,f0,f30 / lwz 0xC4(vptr) / bctrl`, i.e.
// AddBoost(param * lfTimeStep). Slot displacement -> base-header slot, and the
// member offset each one scales:
//     +0x50 -> slot 20 IsInAir      * mfAirEarning        (+0x18)
//     +0x54 -> slot 21 IsDrifting   * mfDriftEarning       (+0x14)
//     +0x58 -> slot 22 IsSpinning   * mfAirSpinEarning     (+0x58)
//     +0x64 -> slot 25 IsOncoming   * mfBoostOnComing      (+0x78)
//     +0x6C -> slot 27 IsTailgating * mfTailgatingEarning  (+0x44)
// AddBoost is +0xC4 == slot 49, the base's protected earn helper; BoostBurnout5
// does not override it, so a plain call here is the same virtual dispatch.
// (The four Feb-2007 pairs -- air/drift/oncoming/tailgating -- pair with exactly
// the tuning params their names predict, which independently corroborates BOTH
// the 50-slot vtable order AND the 34-param member order. The spin pair is new.)
//
// -- burn (0x822C2110-0x822C215C) -------------------------------------------
//   0x822C2110  lbz    r10, 0xC5(r31)       # mbBoosting -- kept in r10 and
//                                           #   re-used at 0x822C2168/0x822C21DC
//   0x822C2118  lfs    f31, flt_82001CC0    # 0.0f
//   0x822C211C  beq    cr6, loc_822C2160    # not boosting -> no burn
//   0x822C2120  lbz    r11, 0xC4(r31)       # mbInfiniteBoost
//   0x822C2128  bne    cr6, loc_822C2160    # infinite -> no burn
//   0x822C212C  lfs    f0,  0xF0(r31)       # mfCurrentCarBoostLossLevel
//   0x822C2134  bne    cr6, loc_822C2148    # != 0.0f -> the loss level wins
//   0x822C2138  lfs    f0,  0xA0(r31)       # mfBoostAmount
//   0x822C213C  lfs    f13, 0x70(r31)       # mfBurnRateBoost
//   0x822C2140  fnmsubs f0, f13, f30, f0    # = mfBoostAmount - mfBurnRateBoost*dt
//   0x822C2148  lfs    f13, 0xA0(r31)
//   0x822C214C  fnmsubs f0, f0, f30, f13    # = mfBoostAmount - lossLevel*dt
//   0x822C2150  stfs   f0,  0xA0(r31)
//   0x822C2158  bge    cr6, loc_822C2160
//   0x822C215C  stfs   f31, 0xA0(r31)       # clamp at 0
//
// -- the request gate (0x822C2168-0x822C2258) --------------------------------
//   0x822C2168  cmplwi cr6, r10, 0          # mbBoosting (the pre-burn read; the
//                                           #   burn writes only mfBoostAmount,
//                                           #   so a plain member read is equal)
//   0x822C2174  lfs    f13, 0x19C(r31)      # mfTimeBoosting
//   0x822C2178  lfs    f0,  flt_820147F8    # 1.25f
//   0x822C2184  blt    cr6, loc_822C218C    # r11 = mbBoosting && (time < 1.25f)
//   0x822C218C  lbz    r9,  0xC7(r31)       # mbBoostRequested
//   0x822C2194  bne    cr6, loc_822C21C4    # requested -> serve
//   0x822C21A0  bne    cr6, loc_822C21C4    # or still inside the minimum run
//   0x822C21A4  lfs    f0,  0xA0(r31)       # -- NOT-SERVED arm --
//   0x822C21A8  stb    r30(1), 0x140(r31)   # mbBoostInterrupted = true
//   0x822C21AC  lfs    f13, 0xA4(r31)       # mfMaxBoost
//   0x822C21B0  stb    r29(0), 0xC5(r31)    # mbBoosting = false
//   0x822C21B8  beq    cr6, loc_822C225C    # bar full -> keep the mode
//   0x822C21BC  stb    r30(1), 0x136(r31)   # mbSwicthBlueToRed = true
//   0x822C21C4  lwz    r11, 0x130(r31)      # -- SERVED arm -- meBoostMode
//   0x822C21C8  cmpwi  cr6, r11, 0          # tested against 0 == E_BOOSTMODE_B3_RED
//   0x822C21CC  bne    cr6, loc_822C220C    #   -> anything else takes the blue arm
//   0x822C21D0  lfs    f0,  0xA0(r31)       # -- RED --
//   0x822C21D8  ble    cr6, loc_822C2204    # no boost left -> stop
//   0x822C21DC  cmplwi cr6, r10, 0          # already boosting?
//   0x822C21E0  bne    cr6, loc_822C21FC
//   0x822C21E4  lfs    f13, 0x100(r31)      # mfMinBoostAllowedAmount
//   0x822C21EC  ble    cr6, loc_822C21F4
//   0x822C21F0  stb    r30(1), 0xC5(r31)    # mbBoosting = true
//   0x822C21F4  stfs   f31, 0x19C(r31)      # mfTimeBoosting = 0.0f
//   0x822C21F8  b      loc_822C2250
//   0x822C21FC  stb    r30(1), 0xC5(r31)    # (already boosting) mbBoosting = true
//   0x822C2200  b      loc_822C2250
//   0x822C2204  stb    r29(0), 0xC5(r31)    # mbBoosting = false; NO time tick
//   0x822C2208  b      loc_822C225C
//   0x822C220C  mr     r3, r31              # -- BLUE --
//   0x822C2210  bl     BoostBurnout5::BlueModeRequestBoost
//   0x822C2214  lbz    r11, 0xC5(r31)       # mbBoosting (RE-read after the call)
//   0x822C221C  bne    cr6, loc_822C2224
//   0x822C2220  stfs   f31, 0x19C(r31)      # mfTimeBoosting = 0.0f
//   0x822C2228  cmplwi cr6, r11, 0          # the returned bool
//   0x822C222C  beq    cr6, loc_822C224C
//   0x822C2230  lfs    f0,  0xA0(r31)
//   0x822C223C  bgt    cr6, loc_822C2244    # mbBoosting = (mfBoostAmount > 0.0f)
//   0x822C2244  stb    r11, 0xC5(r31)
//   0x822C224C  stb    r30(1), 0x136(r31)   # mbSwicthBlueToRed = true
//   0x822C2250  lfs    f0,  0x19C(r31)      # loc_822C2250: mfTimeBoosting += dt
//   0x822C2258  stfs   f0,  0x19C(r31)
//   0x822C225C  ...                         # loc_822C225C: the param reload
//
// DIVERGENCES FROM Feb-2007 (BrnBoostBurnout5.cpp:367-423) -- the asm wins,
// every one of them walked against the listing above:
//   * the retail signature is (f32) only -- Feb-2007's `CarOffenceManager*`
//     second parameter is gone (the base's slot-1 call @0x822F8160 passes only
//     f1, and this prologue keeps only r3 and f1).
//   * Feb-2007 earns through the helper `AddBoostB5`, which diverted blue-mode
//     gains into mfHiddenBoost. That helper does NOT exist in retail (absent
//     from the DecFIGS method list and from the X360 ledger); all five earning
//     sites dispatch slot 49, i.e. the base AddBoost @0x822C0E10, directly.
//   * retail adds a fifth earning source, IsSpinning * mfAirSpinEarning.
//   * the burn is no longer a flat `RemoveBoost(dt * mfUsageDecrease)`: a
//     non-zero mfCurrentCarBoostLossLevel (the per-car boost stat written by
//     SetCarStatBoostLevel @0x822D52D0) OVERRIDES the tuned mfBurnRateBoost.
//   * the request gate is no longer `if (mbBoostRequested)`: a run that has
//     lasted less than 1.25 s keeps being served after the request is released.
//   * Feb-2007's `if (mbInfiniteBoost) mbBoosting = true;` inside the served arm
//     is gone -- infinite boost now only suppresses the burn.
//   * the red arm gained the mfMinBoostAllowedAmount floor and the mfTimeBoosting
//     restart; the blue arm gained the mbSwicthBlueToRed flag on refusal, and the
//     not-served arm sets that flag too whenever the bar is not full.
//   * the trailing `RecalculateBoostMode()` call is NOT in the retail body (no
//     `bl`, and no meBoostMode store anywhere in the function -- which is why
//     BrnBoostBurnout5.h deliberately does not declare it). What retail does in
//     its place is re-read the whole boostparamsasset every update; see the tail.
//
// NaN polarity, checked at all six float compares. The burn clamp is
// `fcmpu + bge` around the zero store, so the store happens ONLY on
// ordered-less -- exactly C++ `< 0.0f` (which is what RemoveBoost's own clamp
// spells). The `blt` on mfTimeBoosting and the `ble`/`bgt` on mfBoostAmount all
// take their true-exit on the ordered case, matching C++ `<` / `>` as written,
// with the `ble` pairs falling through to the not-greater arm exactly as C++
// `>` does for an unordered operand. The one `!=` (mfBoostAmount vs mfMaxBoost,
// `fcmpu + beq` skipping the store) is C++ `!=`, true for unordered -- same as
// the taken branch. No predicate needed negating.
// ---------------------------------------------------------------------------
void
BoostBurnout5::ApplyUpdate(f32 lfTimeStep)
{
    if (IsInAir())
    {
        AddBoost(mfAirEarning * lfTimeStep);
    }
    if (IsDrifting())
    {
        AddBoost(mfDriftEarning * lfTimeStep);
    }
    if (IsSpinning())
    {
        AddBoost(mfAirSpinEarning * lfTimeStep);
    }
    if (IsOncoming())
    {
        AddBoost(mfBoostOnComing * lfTimeStep);
    }
    if (IsTailgating())
    {
        AddBoost(mfTailgatingEarning * lfTimeStep);
    }

    if (mbBoosting && !mbInfiniteBoost)
    {
        // The per-car boost-loss stat overrides the tuned burn rate when set.
        const f32 lfBurnRate = (mfCurrentCarBoostLossLevel == 0.0f)
                             ? mfBurnRateBoost
                             : mfCurrentCarBoostLossLevel;

        // 0x822C2150-0x822C215C is BoostStrategy::RemoveBoost inlined: subtract,
        // store, and clamp mfBoostAmount at zero. De-inlined per the AGENTS.md
        // inlining-reversal rule -- RemoveBoost has no standalone X360 symbol
        // (BrnBoostStrategy.h:331-334) and this site is one of its recovery
        // witnesses; the base .cpp wave owns the body.
        RemoveBoost(lfBurnRate * lfTimeStep);
    }

    // 1.25f == flt_820147F8, written as the literal the image carries. The
    // obvious candidate name is BrnWorld::KF_MIN_BOOST_TIME (DWARF
    // BrnBoostStrategy.h:49, declared extern-without-value at
    // BrnBoostStrategy.h:127 for the base .cpp wave to pin) -- but that
    // identification is NOT proven from this body, so the name is not used here.
    const bool lbInsideMinimumRun = mbBoosting && (mfTimeBoosting < 1.25f);

    if (mbBoostRequested || lbInsideMinimumRun)
    {
        // 0x822C21C8 compares meBoostMode against 0 == E_BOOSTMODE_B3_RED and
        // branches away on "not equal", so RED is the tested arm and everything
        // else falls into the blue path. (Contrast AreWeAllowedToBoost
        // @0x822A6BD8, which really does test both enumerators separately.)
        if (meBoostMode == E_BOOSTMODE_B3_RED)
        {
            if (mfBoostAmount > 0.0f)
            {
                if (mbBoosting)
                {
                    // Already inside a run: the console re-stores the flag
                    // (@0x822C21FC) and does NOT restart the run timer.
                    mbBoosting = true;
                }
                else
                {
                    // Starting a fresh run: the one-chunk floor applies and the
                    // run timer restarts.
                    if (mfBoostAmount > mfMinBoostAllowedAmount)
                    {
                        mbBoosting = true;
                    }

                    mfTimeBoosting = 0.0f;
                }

                mfTimeBoosting += lfTimeStep;
            }
            else
            {
                // Bar empty: stop, and do NOT tick the run timer (@0x822C2208
                // jumps past loc_822C2250).
                mbBoosting = false;
            }
        }
        else
        {
            const bool lbBoostGranted = BlueModeRequestBoost();

            // mbBoosting is re-read AFTER the call (@0x822C2214), which is what
            // the source spells. Nothing in the call chain actually changes it:
            // BlueModeRequestBoost @0x822A6CA8 stores only to +0x13C/+0x140/
            // +0x144/+0x135/+0xCB, and the AddBoost @0x822C0E10 it dispatches
            // has exactly one store, `stfs f0,0xA0(r3)` @0x822C0EA4, and makes
            // no calls of its own. So the reload is the compiler reloading
            // across a call barrier, not a value change -- reading the member
            // here is equivalent either way.
            if (!mbBoosting)
            {
                mfTimeBoosting = 0.0f;
            }

            if (lbBoostGranted)
            {
                mbBoosting = (mfBoostAmount > 0.0f);
            }
            else
            {
                mbSwicthBlueToRed = true;
            }

            mfTimeBoosting += lfTimeStep;
        }
    }
    else
    {
        mbBoostInterrupted = true;
        mbBoosting = false;

        if (mfBoostAmount != mfMaxBoost)
        {
            mbSwicthBlueToRed = true;
        }
    }

    // -- the tuning-param reload (0x822C225C-0x822C23C8) ---------------------
    // Retail re-reads the whole 34-parameter boostparamsasset EVERY update. The
    // block is the one BoostBurnout5::Prepare @0x822C1E90-0x822C1F88 carries
    // (same GUID, same 34 record->member stores, same order) MINUS Prepare's
    // re-zeroing of the three stunt earnings afterwards -- i.e. a live-tuning
    // reload, and the only thing standing where Feb-2007 called
    // RecalculateBoostMode(). It is written inline because the block has no
    // symbol and no declaration of its own anywhere (not in the DecFIGS method
    // list, not in the X360 ledger); BoostBurnout2::ApplyUpdate @0x822C1128
    // carries the same tail, so the original very likely had a shared helper,
    // but naming one here would be an invention.
    //
    // Buffer size: `_BYTE v17[512]` in the DWARF-backed local list for this
    // function, and the same 512 the sibling ConvertI64ToA scratch buffer in
    // CgsAttribSys::AttribSysCollectionKey::GetHashKey carries (DWARF cpp:81).
    char lacGuidText[512];
    rw::core::stdc::ConvertI64ToA(KI_DEFAULT_STUNT_BOOST_PARAMS_GUID, lacGuidText, 10);

    // 0x822C2274-0x822C2284: `bl Attrib__StringToKey / mr r4,r3 / li r5,0`.
    //
    // The key is LIVE, not dead: boostparamsasset's ctor @0x822B8C88 never
    // WRITES r4, so the value handed in here arrives whole at FindCollection and
    // is consumed there as the COLLECTION key (full derivation in
    // boostparamsasset.h). `mr r4,r3` is a 64-bit move with no clrldi,
    // Attrib::StringToKey returns the full 64-bit hash, and the ctor parameter
    // is u64 -- so nothing is narrowed. A static_cast<u32> here would silently
    // drop the high word and make every collection lookup miss, which is exactly
    // the defect that was fixed on this class's declaration.
    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacGuidText), nullptr);

    // 0x822C2288-0x822C23C4 -- the 34 record -> member stores, in ascending
    // member order (which is the order the X360 emits them: +0x10 up to +0x94,
    // one store per 4-byte member, no gaps). Each one goes through the generated
    // accessor; the record offsets live in boostparamsasset.h, and the console
    // `lwz r11,4(asset)` layout-block fetch is what that accessor performs.
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();
    mfDriftEarning            = lBoostParams.DriftEarning();
    mfAirEarning              = lBoostParams.AirEarning();
    // The only two Int32 attributes in the record, and the only two stores the
    // X360 converts (lwz + extsw + std/lfd + fcfid + frsp, 0x822C22A4-0x822C22D8).
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning());
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning());
    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();
    mfTakedownEarning         = lBoostParams.TakedownEarning();
    mfShuntEarning            = lBoostParams.ShuntEarning();
    mfSlamEarning             = lBoostParams.SlamEarning();
    mfNudgeEarning            = lBoostParams.NudgeEarning();
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();
    mfGrindingEarning         = lBoostParams.GrindingEarning();
    mfRubbingEarning          = lBoostParams.RubbingEarning();
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();
    mfTrafficCheck            = lBoostParams.TrafficCheck();
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();
    mfCleanLanding            = lBoostParams.CleanLanding();
    mfFakieLanding            = lBoostParams.FakieLanding();
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();
    mfComboModifier           = lBoostParams.ComboModifier();
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();
    mfBoostChainMin           = lBoostParams.BoostChainMin();
    mfBoostOnComing           = lBoostParams.OnComing();
    mfBeingSlammed            = lBoostParams.BeingSlammed();
    // NOTE: unlike Prepare @0x822C1F8C-0x822C1F94, this reload does NOT re-zero
    // the three stunt earnings afterwards -- it stores exactly what the asset
    // ships. (Prepare's three extra `stfs f31` stores have no counterpart in the
    // 0x822C2288-0x822C23C4 run; the tail goes straight to the dtor.)
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();
    mfOnWrecked               = lBoostParams.OnWrecked();

    // 0x822C23C8: `bl Attrib::Instance::~Instance` -- lBoostParams leaving scope.
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout5_wP_16.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout5 -- wave P partfile 16.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (ARTIST asm is rung 1; the DecFIGS
// DWARF gives declaration shape; Feb-2007 is idiom only).
//
// Functions in this partfile:
//   BoostBurnout5::Prepare           @ 0x822C1D58   (vtable slot 0)
//   BoostBurnout5::UpdateStuntBoost  @ 0x822A6F98   (vtable slot 48)
//
// Both bodies were re-walked instruction-by-instruction against the ARTIST
// listing for this landing pass (0x822C1D58..0x822C1FB8 and
// 0x822A6F98..0x822A70BC, both dumped end to end).
// ============================================================================

namespace BrnWorld
{

// The boostparamsasset collection GUID BoostBurnout5 tunes itself from. X360
// @0x822C1E1C-0x822C1E28: `lis r3,8 ; ori r3,r3,0xCA0D` == 0x0008CA0D == 576013,
// handed to ConvertI64ToA with radix 10 (`li r5,0xA` @0x822C1E20), so the key
// text is "576013". Sibling GUID: BoostBurnout2 = 576005 (0x0008CA05,
// BoostBurnout2::Prepare @0x822C0F74).
//
// Feb-2007 had no per-subclass GUID at all: the BASE Prepare built the key from
// KI_DEFAULT_BOOST_PARAMS_GUID and cached it in the base member mBoostVaultKey
// (BrnBoostStrategy.cpp:48-51), and the subclass constructed its own instance
// from that member (BrnBoostBurnout5.cpp:200). Retail removed it (see the
// maPad0 note in BrnBoostStrategy.h), moved the whole load down into each
// subclass, and gave each subclass its own GUID.
static const s64 KI_BURNOUT5_BOOST_PARAMS_GUID = 576013;

// ---------------------------------------------------------------------------
// Prepare @ 0x822C1D58 -- vtable slot 0.
//
// Shape (asm walked end to end):
//   0x822C1D74  bl BoostStrategy::Prepare ; clrlwi r11,r3,24 ; cmplwi cr6,r11,0 ;
//               bne -> continue, else `li r3,0 ; b <epilogue>`     # early-out
//   0x822C1DB0..0x822C1E14   the state reset -- 23 stores. f31 is flt_82001CC0
//               == 0.0f (loaded once @0x822C1DA4) and f0 is flt_820138DC ==
//               50.0f (@0x822C1DCC, Hex-Rays: `*(_R31 + 328) = 50.0`); v0 is
//               `vspltisw v0,0` @0x822C1D90, stored with stvx128 over the four
//               Vector3 members at +0x150/+0x160/+0x170/+0x180.
//   0x822C1E18  bl BoostStrategy::UpdateMaxBoost   # r4 == 0 (li r4,0 @0x822C1DAC)
//   0x822C1E1C  r3 = 0x0008CA0D (576013) ; li r5,0xA ; r4 = the 512-byte stack
//               text buffer ; bl rw::core::stdc::ConvertI64ToA
//   0x822C1E34  bl Attrib::StringToKey                # r3 = the FULL u64 key
//   0x822C1E38  mr r4,r3 ; li r5,0 ; r3 = the stack instance ;
//               bl Attrib::Gen::boostparamsasset::boostparamsasset
//   0x822C1E48  lwz r11, <instance>+4                 # Instance::mpAttributeData
//   0x822C1E50..0x822C1F88   34 record loads -> 34 member stores
//   0x822C1F84  stb r30(0), 0xC3(r31)                 # mbIsBoostFull = false
//   0x822C1F8C..0x822C1F94   the three stunt earnings RE-ZEROED (stfs f31)
//   0x822C1F98  bl Attrib::Instance::~Instance        # lBoostParams scope exit
//   0x822C1F9C  li r3,1                               # return true
//
// THE CONSTRUCTOR KEY IS LIVE, NOT DEAD. `mr r4,r3` @0x822C1E38 is a full 64-bit
// move with no clrldi, and the ctor @0x822B8C88 never WRITES r4 -- which is
// exactly why the caller's key reaches Attrib::FindCollection whole and is
// consumed there as the COLLECTION key. Attrib::StringToKey returns u64
// (AttributeKey.h:47) and the boostparamsasset ctor parameter is u64; there is
// no narrowing cast here and there must never be one (a static_cast<u32> makes
// every collection lookup miss and silently serves the zeroed default record).
// Full derivation: boostparamsasset.h and BrnBoostStrategy.h.
//
// RETAIL vs FEB-2007 (BrnBoostBurnout5.cpp:186-210) -- the asm wins on all of
// these:
//  * Feb-2007 calls BoostStrategy::Prepare() and IGNORES its result. Retail
//    EARLY-OUTS on it: `if (!BoostStrategy::Prepare()) return false;` (the
//    byte-narrowing clrlwi + cmplwi + bne at 0x822C1D78-0x822C1D80).
//  * Feb-2007 sets `miBoostLevel = 0`. RETAIL DOES NOT -- there is no store to
//    +0x104 anywhere in this function. (RemoveAllBoostAndChunks @0x822C2410
//    still zeroes it; Prepare leaves whatever SetBoostSegments /
//    SetCarStatBoostLevel put there.) Do not re-add the assignment.
//  * Feb-2007 loads NO tuning attributes here -- its BASE Prepare
//    (BrnBoostStrategy.cpp:48-63) loaded an eleven-attribute shared set.
//    Retail's base Prepare @0x822A5C48 touches only the runtime state block, so
//    all 34 tuning params are loaded HERE, exactly as in BoostBurnout2::Prepare
//    (its own layout-pointer load is @0x822C0FA0).
//  * Feb-2007: `mfBlueCrashDecrease = 0.5f * mfMaxMaxBoost;`. Retail stores the
//    literal 50.0f. It is a LITERAL in the binary, not a fold of anything this
//    build still has: mfMaxMaxBoost was a Feb-2007 *BoostStrategy member*
//    (Feb-2007 BrnBoostStrategy.h:237, filled from the asset's Max() at
//    Feb-2007 BrnBoostStrategy.cpp:53) that retail removed, so nothing here
//    pins a named constant -- do not attribute the 50.0f to KF_MAX_MAX_BOOST or
//    to any other declared symbol. (It is numerically half the 100.0f retail's
//    base Prepare @0x822A5C48 seeds into mfMaxBoost -- `*(a1 + 164) = 100.0`,
//    164 == 0xA4 -- which is consistent, but consistency is not attestation.)
//  * Feb-2007 passes the base member mBoostVaultKey to the boostparamsasset
//    ctor. Retail removed that member and builds the key inline.
//  * Retail zeroes a whole stunt-tracking block Feb-2007 did not have at all
//    (+0x150..+0x1A2: the four Vector3s, the four stunt floats, the three stunt
//    bools) and three extra mode flags (+0x136/+0x137/+0x138).
//
// THE RE-ZERO AT THE TAIL IS NOT A TRANSCRIPTION SLIP -- RE-CONFIRMED against
// the asm for this landing. mfStuntJumpEarning / mfStuntSmashEarning /
// mfStuntBillBoardEarning are loaded from the record at 0x822C1F58 (rec+0x14 ->
// +0x80), 0x822C1F60 (rec+0x10 -> +0x84) and 0x822C1F68 (rec+0x18 -> +0x88), and
// then immediately overwritten with f31 == 0.0f at 0x822C1F8C / 0x822C1F90 /
// 0x822C1F94. BoostBurnout5 (blue-bar "Burnout 5" rules) pays no stunt-element
// boost through the shared earnings; the shared asset still carries the values
// for the other two strategies. Keep BOTH the loads and the re-zero -- the loads
// are what the binary does, and dropping them would be a silent divergence.
//
// SpeedForMin/MaxEarning are the ONLY two attributes converted int->float (lwz +
// extsw + std/lfd + fcfid + frsp at 0x822C1E68-0x822C1E80 and
// 0x822C1E84-0x822C1E9C); they are attrib Int32 and land at member +0x1C/+0x20,
// the offsets AddBoost @0x822C0E10 lerps between.
// ---------------------------------------------------------------------------
bool
BoostBurnout5::Prepare()
{
    if (!BoostStrategy::Prepare())
    {
        return false;
    }

    // ---- BoostBurnout5's own state, plus the two base fields this owns ------
    // (asm 0x822C1DB0-0x822C1E14; the stores are independent, so they are
    //  grouped by member here rather than kept in the compiler's interleaving.)
    mfBoostAmount = 0.0f;                       // +0x0A0 (base), stfs @0x822C1DB0
    mbChainNotifyPending = false;               // +0x0CB (base), stb  @0x822C1DEC

    meBoostMode = E_BOOSTMODE_B3_RED;           // +0x130 stw @0x822C1DC0
    mbLeaveBlueDueToCrash = false;              // +0x134 stb @0x822C1DC8
    mbLeaveBlueDueToInsufficientHidden = false; // +0x135 stb @0x822C1DD8
    mbSwicthBlueToRed = false;                  // +0x136 stb @0x822C1DF4
    mbAddRemoveChunkMode = false;               // +0x137 stb @0x822C1DFC
    mbAllowBoostEarning = false;                // +0x138 stb @0x822C1E04
    mfHiddenBoost = 0.0f;                       // +0x13C stfs @0x822C1DB8
    mbBoostInterrupted = false;                 // +0x140 stb @0x822C1DE0
    miChainSize = 0;                            // +0x144 stw @0x822C1DE8

    // The literal flt_820138DC (see the RETAIL vs FEB-2007 note above).
    mfBlueCrashDecrease = 50.0f;                // +0x148 stfs @0x822C1DD4

    // The four stvx128 stores of a `vspltisw v0,0` register: whole-vector zero.
    mStuntRollInProgress.SetZero();             // +0x150 stvx128 r11=0x150 @0x822C1DF8
    mvPositionLastFrame.SetZero();              // +0x160 stvx128 r10=0x160 @0x822C1DF0
    mvTakeOffAtVec.SetZero();                   // +0x170 stvx128 r9 =0x170 @0x822C1E00
    mvLandAtVec.SetZero();                      // +0x180 stvx128 r8 =0x180 @0x822C1E08

    mfBearingLastFrame = 0.0f;                  // +0x190 stfs @0x822C1DBC
    mfAngleSoFar = 0.0f;                        // +0x194 stfs @0x822C1DC4
    mfTimeElapsed = 0.0f;                       // +0x198 stfs @0x822C1DDC
    mfTimeBoosting = 0.0f;                      // +0x19C stfs @0x822C1DE4
    mbHandbreakTurnAttempting = false;          // +0x1A0 stb @0x822C1E0C
    mbWasJustInTheAir = false;                  // +0x1A1 stb @0x822C1E10
    mbTestForCleanLanding = false;              // +0x1A2 stb @0x822C1E14

    UpdateMaxBoost(false);

    // The stack text buffer the GUID is printed into. 512 bytes is measured, not
    // assumed: the frame is 0x290 and the buffer starts at r1+0x70 (var_220),
    // with the saved-register area beginning at r1+0x270 (var_20) -- exactly
    // 0x200 bytes of room. Same idiom as CgsAttribSys::AttribSysCollectionKey::
    // GetHashKey @0x82805C20 (`char lacTemp[512]`, DWARF cpp:81).
    char lacBoostKey[512];
    rw::core::stdc::ConvertI64ToA(KI_BURNOUT5_BOOST_PARAMS_GUID, lacBoostKey, 10);

    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacBoostKey));

    // ---- the 34 tuning params, in member order (= asm store order) ----------
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();     // +0x10 <- rec+0x3C @0x822C1E50
    mfDriftEarning            = lBoostParams.DriftEarning();             // +0x14 <- rec+0x54 @0x822C1E58
    mfAirEarning              = lBoostParams.AirEarning();               // +0x18 <- rec+0x84 @0x822C1E60
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning()); // +0x1C <- rec+0x1C (Int32, fcfid @0x822C1E78)
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning()); // +0x20 <- rec+0x20 (Int32, fcfid @0x822C1E94)
    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();    // +0x24 <- rec+0x40 @0x822C1EA0
    mfTakedownEarning         = lBoostParams.TakedownEarning();          // +0x28 <- rec+0x08 @0x822C1EA8
    mfShuntEarning            = lBoostParams.ShuntEarning();             // +0x2C <- rec+0x28 @0x822C1EB0
    mfSlamEarning             = lBoostParams.SlamEarning();              // +0x30 <- rec+0x24 @0x822C1EB8
    mfNudgeEarning            = lBoostParams.NudgeEarning();             // +0x34 <- rec+0x38 @0x822C1EC0
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();      // +0x38 <- rec+0x04 @0x822C1EC8
    mfGrindingEarning         = lBoostParams.GrindingEarning();          // +0x3C <- rec+0x4C @0x822C1ED0
    mfRubbingEarning          = lBoostParams.RubbingEarning();           // +0x40 <- rec+0x2C @0x822C1ED8
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();        // +0x44 <- rec+0x0C @0x822C1EE0
    mfTrafficCheck            = lBoostParams.TrafficCheck();             // +0x48 <- rec+0x00 @0x822C1EE8
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();        // +0x4C <- rec+0x6C @0x822C1EF0
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();      // +0x50 <- rec+0x48 @0x822C1EF8
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();      // +0x54 <- rec+0x44 @0x822C1F00
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();           // +0x58 <- rec+0x80 @0x822C1F08
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();        // +0x5C <- rec+0x7C @0x822C1F10
    mfCleanLanding            = lBoostParams.CleanLanding();             // +0x60 <- rec+0x60 @0x822C1F18
    mfFakieLanding            = lBoostParams.FakieLanding();             // +0x64 <- rec+0x50 @0x822C1F20
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();        // +0x68 <- rec+0x68 @0x822C1F28
    mfComboModifier           = lBoostParams.ComboModifier();            // +0x6C <- rec+0x5C @0x822C1F30
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();            // +0x70 <- rec+0x64 @0x822C1F38
    mfBoostChainMin           = lBoostParams.BoostChainMin();            // +0x74 <- rec+0x70 @0x822C1F40
    mfBoostOnComing           = lBoostParams.OnComing();                 // +0x78 <- rec+0x34 @0x822C1F48
    mfBeingSlammed            = lBoostParams.BeingSlammed();             // +0x7C <- rec+0x78 @0x822C1F50
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();         // +0x80 <- rec+0x14 @0x822C1F58
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();        // +0x84 <- rec+0x10 @0x822C1F60
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();    // +0x88 <- rec+0x18 @0x822C1F68
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();  // +0x8C <- rec+0x58 @0x822C1F70
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();          // +0x90 <- rec+0x74 @0x822C1F78
    mfOnWrecked               = lBoostParams.OnWrecked();                // +0x94 <- rec+0x30 @0x822C1F80

    mbIsBoostFull = false;                      // +0x0C3 (base), stb @0x822C1F84

    // Burnout 5 rules pay nothing for stunt elements through the shared
    // earnings: the three values are read from the asset above and then
    // discarded (asm 0x822C1F8C-0x822C1F94, three stfs of the same f31 == 0.0f).
    mfStuntJumpEarning      = 0.0f;             // +0x80
    mfStuntSmashEarning     = 0.0f;             // +0x84
    mfStuntBillBoardEarning = 0.0f;             // +0x88

    return true;
}

// ---------------------------------------------------------------------------
// UpdateStuntBoost @ 0x822A6F98 -- vtable slot 48 (the base body is overridden
// by all three strategies).
//
// Four independent stunt awards, each gated on its own bit of
// lpCompletedStuntAction->muStuntActionComplete and each paid through the
// VIRTUAL AddBoost (base slot 49, vtable +0xC4 -- `lwz r10,0(r31) ;
// lwz r10,0xC4(r10) ; mtctr ; bctrl`, never a direct call), so BoostBurnout5
// inherits the base AddBoost @0x822C0E10 speed multiplier and [0, mfMaxBoost]
// clamp on every award:
//
//   bit 0 (clrlwi r11,r11,31 @0x822A6FE0)
//        -> AddBoost((f32)miCompletedBarrelRolls * mfBarrelRollEarning)
//           the count is an INT32: `lwz r11,0x18(r30)` @0x822A6FEC then extsw /
//           std / lfd / fcfid / frsp before the fmuls @0x822A7014.
//   bit 1 (rlwinm r11,r11,0,30,30 @0x822A7024)
//        -> AddBoost(mfAirSpinEarning * mfCompletedAirSpinAngle)
//           lfs f0,0x58(r31) / lfs f13,8(r30) / fmuls f1,f0,f13.
//   bit 2 (rlwinm r11,r11,0,29,29 @0x822A7054)
//        -> AddBoost(mfHandbrake180Earning * mfCompletedHandbreakTurnAngle)
//           lfs f0,0x50(r31) / lfs f13,0xC(r30) / fmuls f1,f0,f13.
//   bit 3 (rlwinm r11,r11,0,28,28 @0x822A7084)
//        -> AddBoost(mfCleanLanding)      # flat, lfs f1,0x60(r31), no payload
//
// The bits are NOT mutually exclusive -- each `beq` skips only its own block and
// falls through into the next test, so one completed stunt can pay all four. The
// bit-3 block is the function's tail (`bctrl` @0x822A70A4 then straight into the
// epilogue), which is why Hex-Rays renders it as a separate `return`; it is the
// same fall-through shape as the other three.
//
// Operand order in each fmuls is preserved exactly as encoded (bit 0:
// count * earning; bits 1 and 2: earning * angle). mfBarrelRollEarning /
// mfAirSpinEarning / mfHandbrake180Earning / mfCleanLanding are the base tuning
// params at +0x5C / +0x58 / +0x50 / +0x60 (BrnBoostStrategy.h), which is exactly
// where Prepare above stores rec+0x7C / rec+0x80 / rec+0x48 / rec+0x60.
//
// The assert is the retail one, verbatim: text "lpCompletedStuntAction != NULL"
// (aLpcompletedstu_0), file aDP4B5MainBurno_532, line immediate `li r5, 0x33E`
// == 830. It is NOT an early-out -- `bne cr6, loc_822A6FDC` @0x822A6FB8 skips
// only the assert triple, and the assert path falls straight through EndAssert
// into the same dereferences (a null pointer would fault), so no `return` is
// added here. Identical shape and constants to BoostBurnout2::UpdateStuntBoost
// @0x822A6478 (assert line 772) and BoostBurnout3::UpdateStuntBoost @0x822A6708.
//
// No Feb-2007 counterpart: neither UpdateStuntBoost nor CompletedStuntAction
// exists anywhere in that drop, so this body is read entirely from the retail
// asm.
//
// BIT NAMES DELIBERATELY NOT INVENTED HERE. The four mask VALUES are
// X360-attested (bits 0/1/2/3, from the clrlwi/rlwinm encodings) and each ROLE
// is attested by which tuning parameter it pays. BrnGameActions.h:760-765
// records that muStuntActionComplete is the physics detector's bitfield and
// points at BrnPhysics::EStuntActionComplete (BrnStuntOffencesManagerShared.h)
// as its enumerator home; that header is not included by this TU, so the masks
// are spelled as literals with their role in a comment, exactly as that note
// permits. Tempting near-miss to reject: BrnGameStateTypes.h's EStuntType IS
// used as a bit position elsewhere, but mapping it here would make bit 0
// (E_STUNT_TYPE_SPIN) pay out barrel rolls, contradicting the tuning parameter
// each bit selects -- it is a DIFFERENT mask.
// ---------------------------------------------------------------------------
void
BoostBurnout5::UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction)
{
    // BrnBoostBurnout5.cpp:830 on the console (the assert's line immediate is
    // 0x33E @0x822A6FC4); reported, then execution continues into the
    // dereferences below.
    CGS_ASSERT(lpCompletedStuntAction != nullptr, "lpCompletedStuntAction != NULL");

    if (lpCompletedStuntAction->muStuntActionComplete & 0x1u)        // barrel roll(s)
    {
        AddBoost(static_cast<f32>(lpCompletedStuntAction->miCompletedBarrelRolls) * mfBarrelRollEarning);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & 0x2u)        // air spin
    {
        AddBoost(mfAirSpinEarning * lpCompletedStuntAction->mfCompletedAirSpinAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & 0x4u)        // handbrake turn
    {
        AddBoost(mfHandbrake180Earning * lpCompletedStuntAction->mfCompletedHandbreakTurnAngle);
    }

    if (lpCompletedStuntAction->muStuntActionComplete & 0x8u)        // clean landing
    {
        AddBoost(mfCleanLanding);
    }
}

} // namespace BrnWorld
