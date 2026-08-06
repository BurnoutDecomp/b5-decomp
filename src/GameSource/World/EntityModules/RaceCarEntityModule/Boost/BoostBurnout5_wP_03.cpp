#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

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
