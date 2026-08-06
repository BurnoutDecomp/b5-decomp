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

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

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
