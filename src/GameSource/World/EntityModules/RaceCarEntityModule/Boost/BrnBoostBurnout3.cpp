// GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Created 2026-09-15 by tools/work/fold_partfiles.py --create-parent (b5-decomp issue #20).
// This family had NO parent TU: its bodies lived in 10 wave partfile(s), each
// with its own hand-written mount line in tools/build/build_game_exe.bat. They are folded
// here in MOUNT ORDER; every partfile's own header comment block is kept verbatim above
// its bodies (the address annotations are the evidence trail). No body was edited.
//
// Folded, in mount order:
//     BoostBurnout3_wP_01.cpp
//     BoostBurnout3_wP_02.cpp
//     BoostBurnout3_wP_03.cpp
//     BoostBurnout3_wP_04.cpp
//     BoostBurnout3_wP_06.cpp
//     BoostBurnout3_wP_07.cpp
//     BoostBurnout3_wP_11.cpp
//     BoostBurnout3_wP_15.cpp
//     BoostBurnout3_wP_16.cpp
//     BoostBurnout3_wP_17.cpp
//
// The header of the first of them (BoostBurnout3_wP_01.cpp) follows verbatim, as this file's own.

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

// the union of the BoostBurnout3_w*.cpp partfiles' #include lines, first occurrence wins, mount order (2026-09-15)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"  // BrnPhysics::Vehicle::EImpactType enumerators
#include "GameSource/GameState/BrnGameStateTypes.h"   // BrnGameState::StuntElementType
#include "rw/math/fpu/scalar_operation.h"   // rw::math::fpu::Clamp -- the console's
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint, gxMessageFilterFlags
#include "GameSource/AttribSys/Generated/classes/boostparamsasset.h"            // Attrib::Gen::boostparamsasset
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
#include "rw/core/stdc/stdc.h"                                                  // rw::core::stdc::ConvertI64ToA
#include "types.hpp"                                                                 // f32 / s64
#include "GameSource/GameState/BrnGameActions.h"             // BrnGameState::GameStateModuleIO::CompletedStuntAction
#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h" // BrnPhysics::EStuntActionComplete
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (the X360 Begin/Fire/EndAssert triple)

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_01.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// Its header is THIS FILE'S header, at the top -- not repeated here.
// ============================================================================

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

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_02.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 02.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//     OnCrash        @ 0x822A68D8
//     OnDriveThru    @ 0x822A6A48
//     OnEndCrashPlay @ 0x822A6AF8
//
// Shared facts walked out of the asm for all three bodies:
//   * The vtable slot the three of them call is +0xC8 = ordinal 50, i.e. the FIRST
//     slot PAST the 50-slot BoostStrategy vtable (0x00..0xC4). That is B3's own
//     `virtual void UpdateMaxBoost(bool)` (@0x822C1C80, DWARF BrnBoostBurnout3.h:118),
//     which HIDES the base's non-virtual BoostStrategy::UpdateMaxBoost. An unqualified
//     `UpdateMaxBoost(...)` inside BoostBurnout3 therefore reproduces the observed
//     virtual dispatch; do NOT qualify it with BoostStrategy::.
//   * B3's own members sit past the 0x130-byte base: +0x130 mfBoostChunkAmount,
//     +0x134 miBoostLevel, +0x138 miOldBoostLevel, +0x13C mfTimeBoosting.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnCrash @ 0x822A68D8   (vtable slot 7)
//
//   0x822A68EC  li    r4, 0                 -> UpdateMaxBoost(false)
//   0x822A68F4  lwz   r11, 0xC8(vtbl)          (virtual, B3 slot 50)
//   0x822A6910  stb   r11(=0), 0xC5(this)   -> mbBoosting = false  (AFTER the call:
//                                              the compiler could not hoist a member
//                                              store across the virtual dispatch, so
//                                              the source order is call-then-store)
//   0x822A690C  fcmpu f13(=+0xA0), f0(=+0xA4)
//   0x822A6914  ble   -> skip
//   0x822A6918  stfs  f0, 0xA0(this)        -> mfBoostAmount = mfMaxBoost
//
// NaN polarity: fcmpu+ble is TAKEN when unordered, i.e. the store is SKIPPED for a
// NaN mfBoostAmount -- which is exactly what plain `>` does in C++. No inversion
// needed here.
//
// DIVERGENCE from Feb-2007: the leaked body decrements miBoostLevel before the
// recompute and has no mbBoosting store. Retail does NEITHER decrement NOR anything
// to miBoostLevel here, and DOES clear mbBoosting. The asm wins.
// ---------------------------------------------------------------------------
void BoostBurnout3::OnCrash()
{
    UpdateMaxBoost( false );

    mbBoosting = false;

    if( mfBoostAmount > mfMaxBoost )
    {
        mfBoostAmount = mfMaxBoost;
    }
}

// ---------------------------------------------------------------------------
// OnDriveThru @ 0x822A6A48   (vtable slot 46 -- pure in the base)
//
//   0x822A6A48  lwz   r11, 0x134(r3)        -> miBoostLevel
//   0x822A6A4C  cmpwi r11, 3                   3 == KI_BOOST_LEVELS (DWARF
//   0x822A6A50  bge   -> skip                  BrnBoostBurnout3.h:134 == 3; note
//   0x822A6A54  addi  r11, r11, 1              Feb-2007 had 5 and compared
//   0x822A6A58  stw   r11, 0x134(r3)           `< KI_BOOST_LEVELS - 1`)
//   0x822A6A5C  lwz   r11, 0xC8(vtbl)       -> tail call UpdateMaxBoost(true)
//   0x822A6A60  li    r4, 1                    true == "fill mfBoostAmount to the
//   0x822A6A6C  bctr                            new mfMaxBoost" (see 0x822C1D34)
//
// The comparison is signed (cmpwi), so a negative miBoostLevel still increments.
// No Feb-2007 counterpart exists for this function -- it is retail-only.
// ---------------------------------------------------------------------------
void BoostBurnout3::OnDriveThru()
{
    if( miBoostLevel < KI_BOOST_LEVELS )
    {
        ++miBoostLevel;
    }

    UpdateMaxBoost( true );
}

// ---------------------------------------------------------------------------
// OnEndCrashPlay @ 0x822A6AF8   (vtable slot 14)
//
//   0x822A6B0C  lwz   r11, 0x138(this)      -> miOldBoostLevel
//   0x822A6B10  cmpwi r11, 1
//   0x822A6B14  bge   -> skip                  (signed)
//   0x822A6B18  li    r11, 1
//   0x822A6B1C  stw   r11, 0x138(this)      -> miOldBoostLevel = 1
//   0x822A6B20  lwz   r11, 0x138(this)         (re-load; plain assignment)
//   0x822A6B30  stw   r11, 0x134(this)      -> miBoostLevel = miOldBoostLevel
//   0x822A6B24  li    r4, 0                 -> UpdateMaxBoost(false)
//   0x822A6B34  lwz   r11, 0xC8(r10)           (B3 slot 50, NOT a tail call)
//   0x822A6B44  lfs   f13, 0xA4(this)       -> mfMaxBoost, read AFTER the recompute
//   0x822A6B48  lfs   f0, flt_820147E8         (= 0.15f; the same immediate is used by
//   0x822A6B4C  fmuls f0, f13, f0                BoostStrategy::SetCarStatBoostLevel
//   0x822A6B50  stfs  f0, 0x100(this)           @0x822D5360 and by BoostBurnout2::
//                                               OnEndCrashPlay for the same store)
//
// The pairing with OnStartCrashPlay @0x822A6AA0 confirms miOldBoostLevel's role:
// that function saves miBoostLevel into +0x138, forces miBoostLevel = 3, calls
// UpdateMaxBoost(false) and parks mfMinBoostAllowedAmount at FLT_EPSILON
// (flt_82014460). This one restores the saved level (floored at 1) and re-raises the
// floor to 15% of the recomputed maximum.
//
// The 0.15f literal is left as a literal on purpose: the DWARF for this .cpp declares
// both KF_BOOST_LOSS_SCALE (cpp:37) and KF_MIN_BOOST_LOSS_SCALE (cpp:38) and nothing
// in the image says which one owns this value, so naming it here would be a guess.
//
// DIVERGENCE from Feb-2007: the leaked body is `miBoostLevel = 0; UpdateMaxBoost();`.
// Retail restores miOldBoostLevel instead of zeroing, floors it at 1, and adds the
// mfMinBoostAllowedAmount store. The asm wins.
// ---------------------------------------------------------------------------
void BoostBurnout3::OnEndCrashPlay()
{
    if( miOldBoostLevel < 1 )
    {
        miOldBoostLevel = 1;
    }

    miBoostLevel = miOldBoostLevel;

    UpdateMaxBoost( false );

    mfMinBoostAllowedAmount = mfMaxBoost * 0.15f;   // flt_820147E8
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_03.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 03.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
//   0x822A6A80  BoostBurnout3::OnEnterInfiniteBoost   (vtable slot 2)
//   0x822A66A8  BoostBurnout3::OnNearMiss             (vtable slot 6)
//   0x822A6960  BoostBurnout3::OnPlayerAttacksRival   (vtable slot 5)
//
// SOURCES (authority order): the X360 ARTIST asm at the three addresses above;
// DecFIGS DWARF BrnBoostBurnout3.h for declaration shape; Feb-2007
// BrnBoostBurnout3.cpp for idiom only -- see the per-body divergence notes
// (two of these three bodies contradict Feb-2007 outright and the third has no
// Feb-2007 counterpart at all).
//
// VTABLE DECODE (X360, 4-byte slots) used by all three bodies:
//   +0xC4 == slot 49 == BoostStrategy::AddBoost(f32)         (protected, base)
//   +0xC8 == slot 50 == BoostBurnout3::UpdateMaxBoost(bool)  (B3's OWN new
//            virtual @0x822C1C80, DWARF B3 h:118, which HIDES the base's
//            NON-virtual BoostStrategy::UpdateMaxBoost -- an unqualified call
//            from a B3 body reproduces the observed indirect dispatch, so do
//            NOT qualify it with BoostStrategy::).
//
// BASE MEMBERS TOUCHED (offsets from BrnBoostStrategy.h):
//   +0x10 mfNearMissBoostEarning   +0x2C mfShuntEarning    +0x30 mfSlamEarning
//   +0x38 mfTradingPaintEarning    +0x3C mfGrindingEarning +0x40 mfRubbingEarning
//   +0x4C mfBoostSlamStrength      +0x8C mfCrashEscapeBoostEarning
//   +0xA0 mfBoostAmount
// ============================================================================

namespace BrnWorld
{

// ============================================================================
// BoostBurnout3::OnEnterInfiniteBoost -- vtable slot 2.  X360 @0x822A6A80.
//
//   lis   r11, flt_82014808@ha      ; the 100.0f rodata slot
//   lwz   r10, 0(r3)                ; vptr
//   li    r4, 0                     ; UpdateMaxBoost's bool argument = false
//   lfs   f0, flt_82014808@l(r11)
//   lwz   r11, 0xC8(r10)            ; vtable +0xC8 == slot 50 == B3::UpdateMaxBoost(bool)
//   stfs  f0, 0xA0(r3)              ; mfBoostAmount = 100.0f
//   mtctr r11 / bctr                ; tail call
//
// Slot arithmetic: 0xC8/4 == 50. BoostStrategy owns slots 0..49 (AddBoost is the
// last, slot 49 == +0xC4 -- see the two AddBoost dispatches below), so +0xC8 is
// the first slot of BoostBurnout3's own vtable extension, i.e. the `virtual void
// UpdateMaxBoost(bool)` that B3 declares (DWARF BrnBoostBurnout3.h:118) and that
// hides the non-virtual BoostStrategy::UpdateMaxBoost. Corroborated by
// OnTakedown @0x822A666C and OnTakenDownByAIOrPlayer @0x822A6954, which both
// dispatch +0xC8 with r4=0 immediately after mutating miBoostLevel (+0x134) --
// exactly the "recompute mfMaxBoost from the boost-level" role of
// BoostBurnout3::UpdateMaxBoost @0x822C1C80 (mfMaxBoost = miBoostLevel *
// 26.666666f, then clamp mfBoostAmount into [0, mfMaxBoost]).
//
// 100.0f is flt_82014808, the SAME merged rodata slot BoostStrategy::Prepare
// @0x822A5C60 loads for `mfMaxBoost = 100.0f`. Written as a literal rather than
// as the base's KF_MAX_MAX_BOOST because constant merging makes the two
// indistinguishable in the image -- do not promote it to a named constant
// without an independent anchor.
//
// DIVERGENCE FROM Feb-2007: BrnBoostBurnout3.cpp:343 has an EMPTY body. Retail
// fills the bar instead (store 100.0f, then let UpdateMaxBoost clamp it down to
// the current mfMaxBoost).
// ============================================================================
void BoostBurnout3::OnEnterInfiniteBoost()
{
    mfBoostAmount = 100.0f;

    UpdateMaxBoost(false);
}

// ============================================================================
// BoostBurnout3::OnNearMiss -- vtable slot 6.  X360 @0x822A66A8.
//
//   cmplwi cr6, r4, 3 / bgtlr cr6   ; UNSIGNED bound: only 0..3 do anything
//   jpt_822A66C4, 4 entries:
//     cases 0,1 -> lfs f1, 0x10(r3) ; mfNearMissBoostEarning
//     cases 2,3 -> lfs f1, 0x8C(r3) ; mfCrashEscapeBoostEarning
//   both then lwz r11, 0xC4(r11) / bctr  ; tail call vtable slot 49 == AddBoost
//
// The compare is `cmplwi` (unsigned), so a negative leNearMissType would also
// fall out through `bgtlr` -- a plain switch with no default reproduces that,
// since no case matches.
//
// DIVERGENCE FROM Feb-2007: BrnBoostBurnout3.cpp:135 takes NO parameter and is a
// bare `AddBoost( mfNearMissEarning );`. Retail added the ENearMissType
// parameter and the crash-escape split; the Feb-2007 member `mfNearMissEarning`
// is retail's `mfNearMissBoostEarning` (+0x10) and the crash-escape reward is a
// separate attrib param at +0x8C.
// ============================================================================
void BoostBurnout3::OnNearMiss(ENearMissType leNearMissType)
{
    switch (leNearMissType)
    {
    case E_NEAR_MISS_NORMAL_TRAFFIC:            // 0
    case E_NEAR_MISS_NORMAL_OTHER_RACE_CAR:     // 1
        AddBoost(mfNearMissBoostEarning);       // +0x10
        break;

    case E_NEAR_MISS_CRASH_ESCAPE_TRAFFIC:          // 2
    case E_NEAR_MISS_CRASH_ESCAPE_OTHER_RACE_CAR:   // 3
        AddBoost(mfCrashEscapeBoostEarning);        // +0x8C
        break;
    }
}

// ============================================================================
// BoostBurnout3::OnPlayerAttacksRival -- vtable slot 5.  X360 @0x822A6960.
//
//   addi   r11, r4, -1
//   cmplwi cr6, r11, 7 / bgtlr cr6   ; UNSIGNED: only leImpactType 1..8 act
//   jpt_822A6980, 8 entries indexed by (leImpactType - 1); every live arm is
//   `lwz r11,0(r3) / lfs f1,<member>(r3) / lwz r11,0xC4(r11) / bctr`
//   i.e. a tail call to vtable slot 49 == BoostStrategy::AddBoost.
//
//   jt idx  leImpactType  target      member loaded
//   ------  ------------  ----------  ---------------------------------------
//     0      1  TRADING_PAINT  0x822A69E0  +0x38 mfTradingPaintEarning
//     1      2  NUDGE          0x822A6A1C  -- bare blr, does nothing
//     2      3  SLAM           0x822A69B8  +0x30 mfSlamEarning
//     3      4  SHUNT          0x822A69CC  +0x2C mfShuntEarning
//     4      5  BOOST_SLAM     0x822A69A4  +0x4C mfBoostSlamStrength
//     5      6  BOOST_SHUNT    0x822A69CC  +0x2C mfShuntEarning  (shares arm 3)
//     6      7  GRINDING       0x822A69F4  +0x3C mfGrindingEarning
//     7      8  RUBBING        0x822A6A08  +0x40 mfRubbingEarning
//
// E_IMPACT_NONE (0) and anything >= E_IMPACT_COUNT (9) fall out of the unsigned
// bound check with no effect. Note that retail deliberately awards NOTHING for a
// nudge even though mfNudgeEarning (+0x34) exists in the tuning block -- that
// member is simply unused by BoostBurnout3.
//
// The boost-slam arm loads mfBoostSlamStrength (attrib "BoostSlamStrength"),
// not mfSlamEarning -- an independent cross-check on the base's +0x4C member
// naming.
//
// NO Feb-2007 COUNTERPART: OnPlayerAttacksRival does not exist anywhere in
// references/Feb-2007/.../Boost/ (grepped) -- it is post-Feb-2007, so the X360
// asm is the only source for this body.
// ============================================================================
void BoostBurnout3::OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType)
{
    switch (leImpactType)
    {
    case BrnPhysics::Vehicle::E_IMPACT_TRADING_PAINT:   // 1
        AddBoost(mfTradingPaintEarning);                // +0x38
        break;

    case BrnPhysics::Vehicle::E_IMPACT_NUDGE:           // 2 -- no reward
        break;

    case BrnPhysics::Vehicle::E_IMPACT_SLAM:            // 3
        AddBoost(mfSlamEarning);                        // +0x30
        break;

    case BrnPhysics::Vehicle::E_IMPACT_SHUNT:           // 4
    case BrnPhysics::Vehicle::E_IMPACT_BOOST_SHUNT:     // 6 (same jump-table arm)
        AddBoost(mfShuntEarning);                       // +0x2C
        break;

    case BrnPhysics::Vehicle::E_IMPACT_BOOST_SLAM:      // 5
        AddBoost(mfBoostSlamStrength);                  // +0x4C
        break;

    case BrnPhysics::Vehicle::E_IMPACT_GRINDING:        // 7
        AddBoost(mfGrindingEarning);                    // +0x3C
        break;

    case BrnPhysics::Vehicle::E_IMPACT_RUBBING:         // 8
        AddBoost(mfRubbingEarning);                     // +0x40
        break;

    default:                                            // E_IMPACT_NONE (0) and >= 9
        break;
    }
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_04.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 04 (three stunt/crash-play hooks).
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
//   0x822A6620  BoostBurnout3::OnPropHit            (vtable slot 16)
//   0x822A6830  BoostBurnout3::OnStuntCompletion    (vtable slot 10)
//   0x822A6AA0  BoostBurnout3::OnStartCrashPlay     (vtable slot 13)
//
// SOURCES (authority order): X360 ARTIST asm at the three addresses above;
// DecFIGS DWARF BrnBoostBurnout3.h (declaration shape + member/constant names);
// Feb-2007 BrnBoostBurnout3.{h,cpp} for idiom only -- see the divergence notes.
//
// VTABLE DECODE (X360 slot offsets, 4-byte slots):
//   +0xC4 == slot 49 == BoostStrategy::AddBoost(f32)          (protected, base)
//   +0xC8 == slot 50 == BoostBurnout3::UpdateMaxBoost(bool)   (B3's OWN new
//            virtual, DWARF B3 h:118 inline `{}`, which HIDES the base's
//            NON-virtual UpdateMaxBoost -- so an unqualified call from a B3
//            body compiles to an indirect call through +0xC8, exactly as the
//            asm shows). BoostBurnout5, which does not redeclare it, instead
//            branches straight to BrnWorld__BoostStrategy__UpdateMaxBoost
//            (@0x822D534C) -- the control confirming the +0xC8 decode is B3's
//            own extension slot and not a base slot.
//
// MEMBER DECODE:
//   base +0x80 mfStuntJumpEarning   +0x84 mfStuntSmashEarning
//   base +0x88 mfStuntBillBoardEarning     base +0x100 mfMinBoostAllowedAmount
//   B3   +0x134 miBoostLevel        B3   +0x138 miOldBoostLevel
//   (B3's miBoostLevel@+0x134 SHADOWS the base's miBoostLevel@+0x104; the base
//   member is the one SetBoostSegments @0x822C0F30 writes. Unqualified uses in
//   a B3 body bind to B3's, which is what the asm touches.)
//
// FEB-2007 DIVERGENCES (asm wins -- documented per body below):
//   * Feb-2007 has NO OnPropHit at all.
//   * Feb-2007 OnStuntCompletion takes no argument and is just `OnTakedown();`.
//     Retail takes a BrnGameState::StuntElementType and branches on it.
//   * Feb-2007 OnStartCrashPlay is `miBoostLevel = KI_BOOST_LEVELS;
//     UpdateMaxBoost();` -- retail additionally saves miOldBoostLevel first and
//     stores mfMinBoostAllowedAmount afterwards, and UpdateMaxBoost takes bool.
//   * Feb-2007 KI_BOOST_LEVELS == 5; DWARF and the retail asm both say 3
//     (0x822A6AB4 `li r10, 3` -> miBoostLevel; 0x822A6858 `cmpwi r11, 3` cap).
// ============================================================================


namespace BrnWorld
{

// ----------------------------------------------------------------------------
// X360 0x822A6620 -- vtable slot 16. A bare tail-call, no stack frame:
//     lwz r11, 0(r3)      ; vptr
//     lfs f1, 0x84(r3)    ; mfStuntSmashEarning
//     lwz r11, 0xC4(r11)  ; AddBoost
//     mtctr r11 / bctr
// Retail-only: the Feb-2007 BoostBurnout3 has no prop-hit hook, and retail's
// OnStuntCompletion deliberately does NOT handle E_STUNT_ELEMENT_TYPE_SMASH --
// the smash earning is paid here instead.
// ----------------------------------------------------------------------------
void
BoostBurnout3::OnPropHit()
{
    AddBoost( mfStuntSmashEarning );
}


// ----------------------------------------------------------------------------
// X360 0x822A6830 -- vtable slot 10. Compare chain (not a jump table):
//     cmpwi r4, 0 ; beq  -> loc_822A68AC : AddBoost(0x80) then return
//     cmpwi r4, 2 ; bne  -> loc_822A68C4 : return, no side effect
//     ; leElementType == 2:
//     lwz  r11, 0x134(r31) ; cmpwi r11, 3 ; bge skip
//     addi r11, r11, 1 ; stw r11, 0x134(r31)
//     lfs  f1, 0x88(r31) ; call vtable+0xC4 (AddBoost)
//     li   r4, 0         ; call vtable+0xC8 (UpdateMaxBoost(false))
// Note the call ORDER: AddBoost BEFORE UpdateMaxBoost here, whereas
// BoostBurnout3::OnTakedown @0x822A6638 calls UpdateMaxBoost (+0xC8) first and
// AddBoost (+0xC4) second. Kept as the asm has it.
// E_STUNT_ELEMENT_TYPE_SMASH (1) falls through with no side effect.
// ----------------------------------------------------------------------------
void
BoostBurnout3::OnStuntCompletion(
    BrnGameState::StuntElementType leElementType )
{
    switch( leElementType )
    {
    case BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP:
        AddBoost( mfStuntJumpEarning );
        break;

    case BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD:
        if( miBoostLevel < KI_BOOST_LEVELS )
        {
            ++miBoostLevel;
        }

        AddBoost( mfStuntBillBoardEarning );

        UpdateMaxBoost( false );
        break;

    default:
        // E_STUNT_ELEMENT_TYPE_SMASH is paid by OnPropHit, not here.
        break;
    }
}


// ----------------------------------------------------------------------------
// X360 0x822A6AA0 -- vtable slot 13.
//     lwz  r11, 0x134(r31)   ; miBoostLevel
//     li   r10, 3            ; KI_BOOST_LEVELS
//     stw  r10, 0x134(r31)   ; miBoostLevel    = KI_BOOST_LEVELS
//     stw  r11, 0x138(r31)   ; miOldBoostLevel = <previous level>
//     li   r4, 0 ; call vtable+0xC8            ; UpdateMaxBoost(false)
//     lfs  f0, flt_82014460 ; stfs f0, 0x100(r31)
// flt_82014460 == 0x34000000 == 1.1920929e-07 == FLT_EPSILON; the same .rdata
// slot is the near-zero spin-angle threshold in BoostStrategy::Update
// @0x822F82CC and the same value BoostBurnout5::OnStartCrashPlay @0x822D5344
// stores into mfMinBoostAllowedAmount. Effect: crash play forces the bar to the
// top segment while leaving essentially no floor on the allowed boost amount.
// ----------------------------------------------------------------------------
void
BoostBurnout3::OnStartCrashPlay()
{
    miOldBoostLevel = miBoostLevel;
    miBoostLevel    = KI_BOOST_LEVELS;

    UpdateMaxBoost( false );

    mfMinBoostAllowedAmount = 1.1920929e-07f;   // flt_82014460 == FLT_EPSILON
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_06.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 06.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::RemoveAllBoostAndChunks @ 0x822A6B68   (vtable slot 41)
//   BoostBurnout3::SetCarStatBoostLevel    @ 0x822C1BC8   (vtable slot 43)
//
// NOT here -- PARKED, see the wave report:
//   BoostBurnout3::Prepare                 @ 0x822C1680   (vtable slot 0)
//   parked at scratchpad/waveP/parked/BoostBurnout3_wP_6.parked.cpp (workflow
//   repo), COMPLETE and asm-walked. Its retail body loads the whole 34-entry
//   boostparamsasset tuning record (`lwz r11,4(asset)` @0x822C1718 then 34
//   fixed-offset lfs/lwz), and the committed Attrib::Gen::boostparamsasset
//   inherits Attrib::Instance PRIVATELY and re-exports nothing, so there is no
//   public way to reach that attribute data area. Unblocking it is ONE line in
//   GameSource/AttribSys/Generated/classes/boostparamsasset.h:
//       using Instance::GetLayoutPointer;
//   (verbatim precedent: cameraexternalbehaviour.h:34). Same blocker the wP_01
//   agent hit on ApplyUpdate.
//
// Vtable note (shared with every BoostBurnout3 partfile): vtable +0xC8 == slot
// 50 == BoostBurnout3's OWN `virtual void UpdateMaxBoost(bool)` (DWARF
// BrnBoostBurnout3.h:118, real X360 body @0x822C1C80), a NEW virtual that HIDES
// the base's non-virtual BoostStrategy::UpdateMaxBoost -- so an unqualified
// UpdateMaxBoost(...) from inside BoostBurnout3 reproduces the observed dispatch.
//
// Member note: the BoostBurnout3-specific members live at +0x130..+0x13C in the
// DecFIGS DWARF order mfBoostChunkAmount / miBoostLevel / miOldBoostLevel /
// mfTimeBoosting. That order is independently confirmed by the STORE KINDS in
// Prepare @0x822C1680 -- stfs/stw/stw/stfs at 0x130/0x134/0x138/0x13C matches
// float/int32/int32/float 4-for-4. BoostBurnout3::miBoostLevel (+0x134)
// deliberately hides BoostStrategy::miBoostLevel (+0x104).
// ============================================================================

                                            // branchless double-fsel clamp

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// RemoveAllBoostAndChunks @ 0x822A6B68 -- vtable slot 41 (pure in the base).
//
//   0x822A6B68  lis   r11, flt_82001CC0@ha
//   0x822A6B6C  lwz   r10, 0(r3)              ; vptr
//   0x822A6B70  li    r9, 1
//   0x822A6B74  li    r4, 0                   ; UpdateMaxBoost's bool = false
//   0x822A6B78  lfs   f0, flt_82001CC0@l(r11) ; 0.0f
//   0x822A6B7C  lwz   r11, 0xC8(r10)          ; slot 50 == B3::UpdateMaxBoost(bool)
//   0x822A6B80  stfs  f0, 0xA0(r3)            ; mfBoostAmount = 0.0f
//   0x822A6B84  stw   r9, 0x134(r3)           ; miBoostLevel = 1
//   0x822A6B88  mtctr r11
//   0x822A6B8C  bctr                          ; TAIL call
//
// DIVERGENCE from Feb-2007 (BrnBoostBurnout3.cpp:383): the leaked body is
// `mfBoostAmount = 0.0f; miBoostLevel = 0; UpdateMaxBoost();`. Retail's floor is
// ONE chunk, not zero -- `li r9,1` is unambiguous -- which is consistent with the
// retail UpdateMaxBoost @0x822C1C80 treating a recomputed max boost of 0.0 as an
// error case (it prints "STOP\n" and forces mfMaxBoost = 1.0f). Level 0 is no
// longer a valid resting state. And UpdateMaxBoost now takes a bool.
// ---------------------------------------------------------------------------
void BoostBurnout3::RemoveAllBoostAndChunks()
{
    mfBoostAmount = 0.0f;

    miBoostLevel = 1;

    UpdateMaxBoost( false );
}

// ---------------------------------------------------------------------------
// SetCarStatBoostLevel @ 0x822C1BC8 -- vtable slot 43.
//
//   0x822C1BE0  extsw r10, r5              ; liBoostLossLevel -> 64-bit
//   0x822C1BE8  li    r4, 0                ; UpdateMaxBoost's bool = false
//   0x822C1BF4  lwz   r9, 0xC8(vptr)       ; slot 50 == B3::UpdateMaxBoost(bool)
//   0x822C1BFC  lfs   f31, flt_82001CC0    ; 0.0f
//   0x822C1C08  lfd/fcfid/frsp             ; (f32)liBoostLossLevel
//   0x822C1C14  stfs  f0, 0xF0(this)       ; mfCurrentCarBoostLossLevel = raw
//   0x822C1C18  fneg  f13, f0
//   0x822C1C1C  fsel  f0, f13, f31, f0     ; lower edge at 0.0f
//   0x822C1C20  lfs   f13, flt_82014808    ; 100.0f (Hex-Rays resolves the literal
//   0x822C1C24  fsubs f12, f13, f0           here AND in the base body @0x822D52DC)
//   0x822C1C28  fsel  f0, f12, f0, f13     ; upper edge at 100.0f
//   0x822C1C2C  stfs  f0, 0xF0(this)       ; mfCurrentCarBoostLossLevel = clamped
//   0x822C1C34  bctrl                      ; UpdateMaxBoost(false)
//   0x822C1C38  lfs   f0, 0xA4(this)       ; mfMaxBoost, read AFTER the recompute
//   0x822C1C3C  lfs   f13, flt_820147FC    ; 0.5f
//   0x822C1C40  fmuls f13, f0, f13
//   0x822C1C44  lfs   f12, flt_820147E8    ; 0.15f
//   0x822C1C48  fmuls f12, f0, f12
//   0x822C1C4C  stfs  f12, 0x100(this)     ; mfMinBoostAllowedAmount
//   0x822C1C50  fneg  f12, f13
//   0x822C1C54  fsel  f13, f12, f31, f13   ; lower edge at 0.0f
//   0x822C1C58  fsubs f12, f0, f13
//   0x822C1C5C  fsel  f0, f12, f13, f0     ; upper edge at mfMaxBoost
//   0x822C1C60  stfs  f0, 0xA0(this)       ; mfBoostAmount
//
// The two fsel pairs are the console's branchless double-fsel clamp, which is
// exactly what rw::math::fpu::Clamp models (its header says so); the same two
// pairs appear verbatim in BoostStrategy::SetCarStatBoostLevel @0x822D5290 and in
// BoostStrategy::UpdateMaxBoost @0x822C0EF4.
//
// ⚠️ liBoostLevel IS UNUSED in this override. r4 carries it in and is overwritten
// with 0 at 0x822C1BE8 (the UpdateMaxBoost argument) before ever being read; only
// r5 is consumed. The BASE body DOES store its first parameter -- `stw r11,0xE8`
// and `stw r11,0x104` at 0x822D52AC/B0 (miCombinedBoostLevel and
// BoostStrategy::miBoostLevel) -- and this override deliberately drops both, so a
// Burnout-3 car's maximum stays driven by its own chunk count (+0x134). The
// parameter is kept and named because the base fixes the slot signature.
//
// The 0.15f and 0.5f literals are left as literals on purpose (same call as the
// wP_02 agent made for the identical flt_820147E8 store in OnEndCrashPlay): the
// DWARF for this .cpp declares KF_BOOST_LOSS_SCALE (cpp:37) and
// KF_MIN_BOOST_LOSS_SCALE (cpp:38), and nothing in the image says which -- if
// either -- owns these values, so naming them would be a guess. All three
// comparands (0.0f / 0.15f / 0.5f / 100.0f) are shared rodata slots that the base
// body loads at the same addresses.
//
// No Feb-2007 counterpart: SetCarStatBoostLevel does not exist in the leaked
// source at all.
// ---------------------------------------------------------------------------
void BoostBurnout3::SetCarStatBoostLevel( s32 liBoostLevel, s32 liBoostLossLevel )
{
    (void)liBoostLevel;   // read by the BASE override, never by this one

    mfCurrentCarBoostLossLevel = static_cast<f32>( liBoostLossLevel );
    mfCurrentCarBoostLossLevel = rw::math::fpu::Clamp( mfCurrentCarBoostLossLevel, 0.0f, 100.0f );

    UpdateMaxBoost( false );

    mfMinBoostAllowedAmount = mfMaxBoost * 0.15f;                                  // flt_820147E8
    mfBoostAmount = rw::math::fpu::Clamp( mfMaxBoost * 0.5f, 0.0f, mfMaxBoost );   // flt_820147FC
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_07.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 07.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::UpdateMaxBoost   @ 0x822C1C80   (vtable slot 50 -- B3's own
//                                                   vtable extension, +0xC8)
//
// NOT here -- lives in the sibling partfile BoostBurnout3_wP_17.cpp:
//   BoostBurnout3::UpdateStuntBoost @ 0x822A6708   (base vtable slot 48, +0xC0)
//   It was parked while BrnGameState::GameStateModuleIO::CompletedStuntAction was
//   only FORWARD-DECLARED (BrnBoostStrategy.h:87); the record now has a real
//   definition in its home, GameSource/GameState/BrnGameActions.h:766, and the
//   body is landed. Do not re-add it here -- one definition only.
//
// Member layout / vtable slot order come from the wave-P keystone header
// BrnBoostStrategy.h; the BoostBurnout3-specific members (+0x130..+0x13C) are
// the DecFIGS DWARF order mfBoostChunkAmount / miBoostLevel / miOldBoostLevel /
// mfTimeBoosting, pinned on X360 by BoostBurnout3::Prepare @0x822C1680
// (stw 2 -> +0x134, stw 0 -> +0x138, stfs 0.0f -> +0x130 and +0x13C).
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// KF_BOOST_CHUNK_AMOUNT -- DecFIGS DWARF BrnBoostBurnout3.h:135, definition at
// BrnBoostBurnout3.cpp:41 (a private `static const float` of BoostBurnout3).
//
// Value from flt_82014A18, the rodata slot UpdateMaxBoost @0x822C1CBC loads.
// That slot has exactly ONE referencing function in the whole image
// (0x822C1C80, this file's body -- checked across all 30,086 exported X360
// functions), so it is NOT a merged shared literal and the DWARF name binding
// is safe here (contrast the 100.0f in partfile 03, which shares flt_82014808
// with BoostStrategy::Prepare and was therefore left a literal).
//
// Corroboration for the name: KI_BOOST_LEVELS == 3 (DWARF h:134, and the retail
// asm's `cmpwi r11, 3` boost-level ceiling in OnTakedown @0x822A6650 -- an
// earlier revision cited @0x822A6A4C here, which is OnDriveThru's copy of the
// same ceiling test, not OnTakedown's), and
// 3 * 26.666666f == 80.0f -- i.e. the constant is exactly one third of the full
// bar, "the amount of boost one chunk is worth". Note 26.666666f and 80.0f/3.0f
// are the SAME f32 (both round to 0x41D5_5555 == 26.66666603088378906), so the
// literal spelling is not recoverable; the value is.
//
// CONDUCTOR: if the TU's constants block (another partfile) also defines this,
// delete one of the two -- `cl /c` cannot see the duplicate, it becomes LNK2005.
// No sibling wave-P partfile defines it as of this writing.
// ---------------------------------------------------------------------------
const f32 BoostBurnout3::KF_BOOST_CHUNK_AMOUNT = 26.666666f;

// ---------------------------------------------------------------------------
// UpdateMaxBoost @ 0x822C1C80 -- vtable slot 50 (+0xC8), the FIRST slot of
// BoostBurnout3's own vtable extension. This is a NEW virtual that HIDES the
// non-virtual BoostStrategy::UpdateMaxBoost(bool) @0x822C0EB0; the base's
// version computes mfMaxBoost from miCombinedBoostLevel (+0xE8) * 10.0f, this
// one from BoostBurnout3's OWN miBoostLevel (+0x134). Both classes declare a
// member spelled `miBoostLevel` (base +0x104, B3 +0x134) and the asm reads
// 0x134, so the unqualified name below correctly resolves to B3's.
//
// Callers -- ELEVEN sites, every one of them a `lwz r11, 0xC8(vptr)` dispatch,
// never a direct bl. Each dispatch address below was re-derived by scanning this
// TU's exported listings for `0xC8(`, and each r4 value is the `li r4, N` that
// immediately precedes it:
//   r4 = 0 : Prepare                 @0x822C16C4  (li r4,0 @0x822C16BC)
//            OnTakedown              @0x822A666C  (li r4,0 @0x822A6664)
//            OnStuntCompletion       @0x822A688C  (li r4,0 @0x822A6884)
//            OnCrash                 @0x822A68F4  (li r4,0 @0x822A68EC)
//            OnTakenDownByAIOrPlayer @0x822A6954  (li r4,0 @0x822A6950)
//            OnEnterInfiniteBoost    @0x822A6A90  (li r4,0 @0x822A6A88)
//            OnStartCrashPlay        @0x822A6ACC  (li r4,0 @0x822A6AB8)
//            OnEndCrashPlay          @0x822A6B34  (li r4,0 @0x822A6B24)
//            RemoveAllBoostAndChunks @0x822A6B7C  (li r4,0 @0x822A6B74)
//            SetCarStatBoostLevel    @0x822C1BF4  (li r4,0 @0x822C1BE8)
//   r4 = 1 : OnDriveThru             @0x822A6A64  (li r4,1 @0x822A6A60) -- the
//            ONLY fill-to-max caller. It is the boost-level-UP path (increment
//            miBoostLevel to a ceiling of 3 @0x822A6A4C-58, then refill the bar),
//            which is what names the parameter: Feb-2007 spelled that site
//            `UpdateMaxBoost(); mfBoostAmount = mfMaxBoost;`
//            (BrnBoostBurnout3.cpp:122-123) and retail folded the second line
//            into the callee as this bool.
//   (OnWrecked @0x822C2438 is NOT among them -- it contains no 0xC8 dispatch and
//    no +0x134 access at all. An earlier revision of this comment grouped four of
//    the sites above under an "OnWrecked-family" label; that label was wrong and
//    is removed.)
//
//   0x822C1CA0  lwz    r11, 0x134(r31)          ; miBoostLevel  (B3's own)
//   0x822C1CA4  extsw  r11, r11                 ; signed
//   0x822C1CB4  fcfid  f0, f0                   ; -> f64
//   0x822C1CB8  frsp   f13, f0                  ; -> f32
//   0x822C1CBC  lfs    f0,  flt_82014A18        ; 26.666666f
//   0x822C1CC8  fmuls  f0,  f13, f0
//   0x822C1CCC  stfs   f0,  0xA4(r31)           ; mfMaxBoost =
//   0x822C1CD0  fcmpu  cr6, f0, f31             ; f31 = flt_82001CC0 = 0.0f
//   0x822C1CD4  bne    cr6, loc_822C1D0C        ; NaN also skips (EQ clear)
//   0x822C1CDC  ld     r11, CgsDev::Message::gxMessageFilterFlags
//   0x822C1CE0  clrldi r11, r11, 63             ; & 1
//   0x822C1CE8  beq    cr6, loc_822C1D00
//   0x822C1CFC  bl     CgsDev::StrStreamBase::operator<<(gpDebugPrint,"STOP\n")
// loc_822C1D00:                                 ; reached whether or not it printed
//   0x822C1D04  lfs    f0,  flt_82001C98        ; 1.0f
//   0x822C1D08  stfs   f0,  0xA4(r31)           ; mfMaxBoost = 1.0f
// loc_822C1D0C:
//   0x822C1D0C  lfs    f13, 0xA0(r31)           ; mfBoostAmount
//   0x822C1D14  fneg   f12, f13
//   0x822C1D18  lfs    f0,  0xA4(r31)           ; mfMaxBoost (post-fixup)
//   0x822C1D20  fsel   f13, f12, f31, f13       ; (-amount >= 0) ? 0.0f : amount
//   0x822C1D24  fsubs  f12, f0, f13
//   0x822C1D28  fsel   f13, f12, f13, f0        ; (max-amount >= 0) ? amount : max
//   0x822C1D2C  stfs   f13, 0xA0(r31)
//   0x822C1D30  beq    cr6, loc_822C1D38        ; cr6 still holds `arg == 0`
//   0x822C1D34  stfs   f0,  0xA0(r31)           ; mfBoostAmount = mfMaxBoost
//
// The two fsels are the compiler's branch-free rendering of the source-level
// clamp (de-optimised back to ifs per the project rule). They are NOT a hand-
// written NaN-polarity branch: fsel picks its FALSE operand on unordered, so a
// NaN mfBoostAmount would come out of the pair as mfMaxBoost, whereas the two
// ifs leave it NaN. mfBoostAmount is never NaN at this point in any reachable
// path (every writer -- Prepare, AddBoost, SetBoostAmount, OnEnterInfiniteBoost
// -- stores a clamped finite value), so the source form is the faithful one.
//
// DIVERGENCES FROM Feb-2007 (BrnBoostBurnout3.cpp:330):
//   * Feb-2007: `mfMaxBoost = (miBoostLevel + 1) * mfMaxMaxBoost / KI_BOOST_LEVELS;`
//     with KI_BOOST_LEVELS == 5. Retail: no `+ 1`, no division, no
//     mfMaxMaxBoost -- a straight multiply by the build constant, and
//     KI_BOOST_LEVELS is 3. miBoostLevel's retail range is 1..3, pinned by the
//     two GUARDS rather than by any single store:
//       floor   OnTakenDownByAIOrPlayer @0x822A6934 `cmpwi cr6, r11, 1` + `ble`
//               (decrement only while above 1)
//       ceiling OnTakedown  @0x822A6650 `cmpwi cr6, r11, 3` + `bge`
//               OnDriveThru @0x822A6A4C `cmpwi cr6, r11, 3` + `bge`
//               (increment only while below 3)
//     Unconditional stores agree: RemoveAllBoostAndChunks @0x822A6B84 stores 1,
//     Prepare @0x822C16C8 stores 2, OnStartCrashPlay @0x822A6AC4 stores 3.
//     (An earlier revision of this comment cited "OnWrecked stores 1" as the
//     witness. That was fabricated -- OnWrecked @0x822C2438 never touches +0x134
//     at all. The 1..3 conclusion stands; the witnesses above are the real ones.)
//   * Feb-2007 takes no parameter and has no zero-guard and no clamp; all three
//     are new in retail.
//   * The "STOP\n" debug print is new in retail; DWARF puts a
//     `using namespace CgsDev::Message;` inside this body at cpp:625, which is
//     exactly the gxMessageFilterFlags gate below.
// ---------------------------------------------------------------------------
void BoostBurnout3::UpdateMaxBoost(bool lbFillBoost)
{
    mfMaxBoost = static_cast<f32>(miBoostLevel) * KF_BOOST_CHUNK_AMOUNT;

    if( mfMaxBoost == 0.0f )
    {
        if( CgsDev::Message::gxMessageFilterFlags & 1 )
        {
            *CgsDev::Log::gpDebugPrint << "STOP\n";
        }

        mfMaxBoost = 1.0f;
    }

    if( mfBoostAmount < 0.0f )
    {
        mfBoostAmount = 0.0f;
    }

    if( mfBoostAmount > mfMaxBoost )
    {
        mfBoostAmount = mfMaxBoost;
    }

    if( lbFillBoost )
    {
        mfBoostAmount = mfMaxBoost;
    }
}

}

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_11.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 11.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::ApplyUpdate  @ 0x822C1880   (base vtable slot 1)
//
// This is the body that was parked as
// scratchpad/waveP/parked/BoostBurnout3_wP_1.parked.cpp (workflow repo). Both
// of its parked blockers are gone:
//   * BrnBoostBurnout3.h landed (base 0x130 + four own members at +0x130..+0x13C).
//   * Attrib::Gen::boostparamsasset now exposes the 34 NAMED attribute accessors
//     (DecFIGS DWARF boostparamsasset.h:73-307), so the tuning record is read
//     through the generated API instead of a raw offset poke at the layout block.
//     The parked draft asked for `using Instance::GetLayoutPointer;` and then
//     indexed the block itself; that was rejected -- it pushes the console-vs-host
//     +0x04/+0x08 mpAttributeData widening into a consumer TU and violates
//     AGENTS.md "NO RAW OFFSET POINTER HACKS". The offset knowledge stays in
//     boostparamsasset.h.
//
// TWO DEFECTS IN THE PARKED DRAFT, FIXED HERE (both re-derived from the asm):
//   1. `static_cast<u32>(Attrib::StringToKey(lacGuidText))` -- a LIVE 64-bit
//      truncation. Attrib::StringToKey returns u64 (AttributeKey.h:43/47;
//      @0x82805828 tail-calls the lookup8 hash and returns r3 whole), the console
//      moves it with a full 64-bit `mr r4, r3` @0x822C1A5C (no clrldi), and the
//      boostparamsasset ctor hands r4 straight to Attrib::FindCollection as the
//      COLLECTION key. Narrowed, every lookup misses and all 34 parameters come
//      back from the ctor's zeroed DefaultDataArea(0x88).
//   2. The draft's banner claimed "the key argument is DEAD -- the ctor resolves
//      its collection from the boostparamsasset CLASS key". FALSE, and it is the
//      claim that justified defect 1. The ctor @0x822B8C88 never WRITES r4 --
//      which is exactly why the caller's key is LIVE: between the entry and
//      `bl Attrib__FindCollection` @0x822B8CB8 it writes only r31 (this), r30
//      (owner) and r3 (the whole-doubleword class key, lis/ori + insrdi), so r4
//      reaches FindCollection holding the caller's key. Hex-Rays rendering a
//      one-argument call is an artifact of the un-written register.
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// ApplyUpdate @ 0x822C1880 -- base vtable slot 1, dispatched by
// BoostStrategy::Update @0x822F8160.
//
// SIGNATURE from the asm, not the pseudocode: `(this, f1)` only. r3 = this
// (`mr r31, r3` @0x822C1894), the time step arrives in f1 and is parked in f31
// for the whole body (`fmr f31, f1` @0x822C1898). No GPR past r3 is read on
// entry, so the Feb-2007 second parameter (a CarOffenceManager*) is gone in
// retail. (PPC float-arg ABI: the f32 travels in an FPR and skips its GPR slot,
// which is why there is no r4 use to look for.)
//
// SHAPE -- four earning sources, then the burn step, then an unconditional
// re-read of the whole 34-value boostparamsasset record.
//
// RETAIL vs FEB-2007 (all four asm-driven):
//   (a) a FOURTH earning source, tailgating (slot 27 @0x822C1954, scaled by
//       mfTailgatingEarning +0x44). Feb-2007 has only air/drift/oncoming.
//   (b) the boost-request test gained a MINIMUM-BOOST-TIME latch: once boosting,
//       boost keeps running while mfTimeBoosting is under 1.25 s even after the
//       request drops (0x822C199C-0x822C19B8). Feb-2007 drops boost the instant
//       mbBoostRequested goes false.
//   (c) the drain rate is no longer a flat constant: it is the per-car
//       mfCurrentCarBoostLossLevel (+0xF0, seeded by SetCarStatBoostLevel),
//       falling back to the attrib mfBurnRateBoost (+0x70) when that is exactly
//       zero (0x822C1A14-0x822C1A28).
//   (d) the whole boostparamsasset record is re-read at the END OF EVERY UPDATE
//       (loc_822C1A40, reached from both the boosting and the not-boosting
//       paths). Not in Feb-2007 at all, and the reason this function carries a
//       0x290-byte stack frame.
//
// NaN polarity, taken from the branch senses rather than transliterated:
//   * `fcmpu f13(mfTimeBoosting), 1.25 ; blt` @0x822C19AC -- taken only when
//     ORDERED-less, so a NaN clears the latch: `mfTimeBoosting < 1.25f`.
//   * `fcmpu f12(mfBoostAmount), 0.0 ; bgt` @0x822C19F0 -- ordered-greater only,
//     so a NaN takes the "no boost left" path: `mfBoostAmount > 0.0f`.
//   * `fcmpu f13(mfCurrentCarBoostLossLevel), 0.0 ; bne` @0x822C1A20 -- `bne` is
//     taken when UNORDERED too, so a NaN keeps the per-car rate. C++ `==` is
//     false for NaN, so `if (rate == 0.0f) rate = fallback;` matches.
//   * `fcmpu f13(new mfBoostAmount), 0.0 ; bge` @0x822C1A34 -- `bge` tests LT==0
//     and is taken when unordered, so a NaN is NOT clamped. C++ `<` is false for
//     NaN, so `if (amount < 0.0f) amount = 0.0f;` matches.
// All four match the plain C++ spelling used below.
// ---------------------------------------------------------------------------
void BoostBurnout3::ApplyUpdate(f32 lfTimeStep)
{
    // ---- earning ---------------------------------------------------------
    // Each source is `bctrl` through the base getter's slot, then
    // `lfs <param> ; fmuls f1, f0, f31 ; bctrl` through slot 49 (AddBoost,
    // console vtable +0xC4).
    if (IsInAir())                                          // slot 20, +0x50 @0x822C18A0
    {
        AddBoost(mfAirEarning * lfTimeStep);                // lfs f0, 0x18(r31)
    }

    if (IsDrifting())                                       // slot 21, +0x54 @0x822C18DC
    {
        AddBoost(mfDriftEarning * lfTimeStep);              // lfs f0, 0x14(r31)
    }

    if (IsOncoming())                                       // slot 25, +0x64 @0x822C1918
    {
        AddBoost(mfBoostOnComing * lfTimeStep);             // lfs f0, 0x78(r31)
    }

    if (IsTailgating())                                     // slot 27, +0x6C @0x822C1954
    {
        AddBoost(mfTailgatingEarning * lfTimeStep);         // lfs f0, 0x44(r31)
    }

    // ---- burning ---------------------------------------------------------
    // `lbz r10, 0xC5(r31)` @0x822C1988 -- mbBoosting is sampled ONCE into r10,
    // and that pre-update value is what BOTH the latch test @0x822C1994 and the
    // "boost has just started" test @0x822C1A00 use. (No store to +0xC5 can
    // precede either test: the two stores at 0x822C19DC / 0x822C19F8 both branch
    // straight to loc_822C1A40.)
    const bool lbWasBoosting = mbBoosting;

    // The minimum-boost-time latch (r11 @0x822C199C-0x822C19B8): already
    // boosting, and the current burst is younger than 1.25 s.
    //
    // 1.25f is flt_820147F8, loaded by all three strategies' ApplyUpdate
    // (B2 @0x822C1340, B3 here, B5 @0x822C2170) -- consistent with the shared
    // BrnBoostStrategy.h-level KF_MIN_BOOST_TIME (h:49), whose value the
    // BrnBoostStrategy.cpp wave has not pinned. It is NOT spelled as that name
    // here because the .rdata float region is not a per-header constant array:
    // flt_820147E8, four slots earlier, is a BrnBoostBurnout3.cpp-local constant
    // (0.15f; loaded by OnEndCrashPlay @0x822A6B40/0x822A6B48 -- landed as a
    // literal in BoostBurnout3_wP_02.cpp -- and again by SetCarStatBoostLevel
    // @0x822C1C44, BoostBurnout3_wP_06.cpp), so position in that pool identifies
    // nothing. (RemoveAllBoostAndChunks @0x822A6B68 never touches flt_820147E8;
    // it loads flt_82001CC0 == 0.0f.)
    // The literal the asm actually loads is written instead of asserting an
    // identification this TU cannot prove.
    const bool lbWithinMinBoostTime = lbWasBoosting && (mfTimeBoosting < 1.25f);

    if (mbBoostRequested || lbWithinMinBoostTime)           // lbz 0xC7 ; bne / clrlwi r11
    {
        if (mbInfiniteBoost)                                // lbz 0xC4 @0x822C19D0
        {
            mbBoosting = true;                              // stb r8(=1), 0xC5
        }
        else if (mfBoostAmount > 0.0f)                      // lfs 0xA0 ; fcmpu ; bgt
        {
            // A fresh burst restarts the timer (`cmplwi r10,0 ; bne` @0x822C1A00
            // skips the zero-store when we were already boosting).
            if (!lbWasBoosting)
            {
                mfTimeBoosting = 0.0f;                      // stfs f0(=0.0), 0x13C
            }
            mfTimeBoosting += lfTimeStep;                   // lfs/fadds f31/stfs 0x13C
            mbBoosting = true;                              // stb r8(=1), 0xC5

            // Per-car drain rate, with the attrib default as the fallback.
            f32 lfBoostBurnRate = mfCurrentCarBoostLossLevel;   // lfs f13, 0xF0
            if (lfBoostBurnRate == 0.0f)                        // fcmpu ; bne
            {
                lfBoostBurnRate = mfBurnRateBoost;              // lfs f13, 0x70
            }

            // 0x822C1A2C fnmsubs f13, f13, f31, f12 (== mfBoostAmount - rate*dt)
            // + 0x822C1A30 stfs 0xA0 + 0x822C1A34 fcmpu / `bge` @0x822C1A38 /
            // 0x822C1A3C stfs 0.0. That subtract-then-clamp-at-zero pair IS
            // BoostStrategy::RemoveBoost (no standalone X360 symbol -- inlined at
            // every call site). Reversing the inline restores the call, matching
            // the landed sibling partfiles BoostBurnout2_wP_11 and _wP_05.
            // NaN note unchanged: RemoveBoost's own `<` is false for NaN, exactly
            // as `bge` is taken when the compare is unordered.
            RemoveBoost(lfBoostBurnRate * lfTimeStep);
        }
        else
        {
            mbBoosting = false;                             // stb r9(=0), 0xC5 (loc_822C19F8)
        }
    }
    else
    {
        mbBoosting = false;                                 // stb r9(=0), 0xC5 (loc_822C19F8)
    }

    // ---- re-read the boost tuning parameters (loc_822C1A40) ---------------
    // Unconditional, every update. Print the asset GUID as decimal text, hash
    // the text into a collection key, construct a stack boostparamsasset over
    // it, then copy the 34-slot (0x88-byte) record into the base's 34 tuning
    // members. The same block runs in BoostBurnout3::Prepare @0x822C1680.
    //
    // 576011 == 0x8CA0B, staged as `lis r3,8 ; ori r3,r3,0xCA0B` @0x822C1A40/4C.
    // The DecFIGS DWARF names it BrnWorld::KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID
    // (BrnBoostBurnout3.cpp:43, `const int64_t = 576011`). That namespace-scope
    // definition belongs to whichever partfile lands Prepare -- spelling it here
    // too would duplicate it -- so the literal is written out.
    //
    // The 512-byte text buffer is not DWARF-attested for THIS function (the
    // dwarfdump for BrnBoostBurnout3.cpp carries only the GUID constant). The
    // console frame bounds it to 512..544 bytes (buffer at sp+0x70 inside a
    // 0x290 frame, minus the callee linkage/parameter-save area at the top), and
    // 512 is the size the DWARF gives the identical ConvertI64ToA-then-hash site
    // CgsAttribSys::AttribSysCollectionKey::GetHashKey (DWARF cpp:81, already
    // landed as `char lacTemp[512]`).
    char lacGuidText[512];                                  // sp+0x70
    rw::core::stdc::ConvertI64ToA(576011, lacGuidText, 10);

    // `bl Attrib__StringToKey` @0x822C1A58 -> `mr r4, r3` @0x822C1A5C: the FULL
    // 64-bit hash, moved whole (no clrldi), is the ctor's collection key.
    // `li r5, 0` @0x822C1A60 is the owner.
    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacGuidText), nullptr);

    // The console loads the layout block once (`lwz r11, 4(inst)` @0x822C1A6C --
    // Attrib::Instance::mpAttributeData) and then reads it at fixed offsets.
    // Those offsets live in boostparamsasset.h, one per named accessor; the
    // record-offset -> member mapping below is the store order of
    // 0x822C1A70..0x822C1BA8, member offset +0x10..+0x94 ascending, 34 for 34.
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();    // rec+0x3C -> +0x10
    mfDriftEarning            = lBoostParams.DriftEarning();            // rec+0x54 -> +0x14
    mfAirEarning              = lBoostParams.AirEarning();              // rec+0x84 -> +0x18

    // The ONLY two Int32 attributes in the record -- the only two slots the
    // console routes through lwz/extsw/fcfid/frsp instead of lfs
    // (0x822C1A88-0x822C1ABC), i.e. a signed int-to-float conversion.
    mfSpeedForMinEarning      = static_cast<f32>(lBoostParams.SpeedForMinEarning()); // rec+0x1C -> +0x1C
    mfSpeedForMaxEarning      = static_cast<f32>(lBoostParams.SpeedForMaxEarning()); // rec+0x20 -> +0x20

    mfMaxSpeedBoostModifier   = lBoostParams.MaxSpeedBoostModifier();   // rec+0x40 -> +0x24
    mfTakedownEarning         = lBoostParams.TakedownEarning();         // rec+0x08 -> +0x28
    mfShuntEarning            = lBoostParams.ShuntEarning();            // rec+0x28 -> +0x2C
    mfSlamEarning             = lBoostParams.SlamEarning();             // rec+0x24 -> +0x30
    mfNudgeEarning            = lBoostParams.NudgeEarning();            // rec+0x38 -> +0x34
    mfTradingPaintEarning     = lBoostParams.TradingPaintEarning();     // rec+0x04 -> +0x38
    mfGrindingEarning         = lBoostParams.GrindingEarning();         // rec+0x4C -> +0x3C
    mfRubbingEarning          = lBoostParams.RubbingEarning();          // rec+0x2C -> +0x40
    mfTailgatingEarning       = lBoostParams.TailgatingEarning();       // rec+0x0C -> +0x44
    mfTrafficCheck            = lBoostParams.TrafficCheck();            // rec+0x00 -> +0x48
    mfBoostSlamStrength       = lBoostParams.BoostSlamStrength();       // rec+0x6C -> +0x4C
    mfHandbrake180Earning     = lBoostParams.Handbrake180Earning();     // rec+0x48 -> +0x50
    mfHandbrake360Earning     = lBoostParams.Handbrake360Earning();     // rec+0x44 -> +0x54
    mfAirSpinEarning          = lBoostParams.AirSpinEarning();          // rec+0x80 -> +0x58
    mfBarrelRollEarning       = lBoostParams.BarrelRollEarning();       // rec+0x7C -> +0x5C
    mfCleanLanding            = lBoostParams.CleanLanding();            // rec+0x60 -> +0x60
    mfFakieLanding            = lBoostParams.FakieLanding();            // rec+0x50 -> +0x64
    mfBoostSpinIncrease       = lBoostParams.BoostSpinIncrease();       // rec+0x68 -> +0x68
    mfComboModifier           = lBoostParams.ComboModifier();           // rec+0x5C -> +0x6C
    mfBurnRateBoost           = lBoostParams.BurnRateBoost();           // rec+0x64 -> +0x70
    mfBoostChainMin           = lBoostParams.BoostChainMin();           // rec+0x70 -> +0x74
    mfBoostOnComing           = lBoostParams.OnComing();                // rec+0x34 -> +0x78
    mfBeingSlammed            = lBoostParams.BeingSlammed();            // rec+0x78 -> +0x7C
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();        // rec+0x14 -> +0x80
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();       // rec+0x10 -> +0x84
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();   // rec+0x18 -> +0x88
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning(); // rec+0x58 -> +0x8C
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();         // rec+0x74 -> +0x90
    mfOnWrecked               = lBoostParams.OnWrecked();               // rec+0x30 -> +0x94

    // `bl Attrib__Instance___Instance` @0x822C1BAC -- lBoostParams leaving scope.
}

}   // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_15.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 15.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::OnTakedown              @ 0x822A6638   (vtable slot 3)
//   BoostBurnout3::OnTakenDownByAIOrPlayer @ 0x822A6930   (vtable slot 4)
//   BoostBurnout3::OnWrecked               @ 0x822C2438   (vtable slot 8)
//
// Slot numbers are the base's (BrnBoostStrategy.h slots 3 / 4 / 8); all three
// are pure in the base, so BoostBurnout3 must define them.
//
// Member layout / vtable slot order come from the wave-P keystone header
// BrnBoostStrategy.h and from BrnBoostBurnout3.h. Offsets touched here:
//   +0x028 BoostStrategy::mfTakedownEarning        (attrib rec+0x08)
//   +0x094 BoostStrategy::mfOnWrecked              (attrib rec+0x30)
//   +0x0A0 BoostStrategy::mfBoostAmount
//   +0x0A4 BoostStrategy::mfMaxBoost
//   +0x0CA BoostStrategy::mbJustLostBoostChunk
//   +0x100 BoostStrategy::mfMinBoostAllowedAmount
//   +0x134 BoostBurnout3::miBoostLevel   -- B3's OWN level member; it HIDES the
//          base's miBoostLevel at +0x104, and every B3 body uses +0x134.
//
// Two rodata literals are used below; both are pinned, not guessed:
//   flt_82001CC0 == 0.0f  -- BoostBurnout3::UpdateMaxBoost @0x822C1CC4 loads it
//          into f31 and compares mfMaxBoost against it at 0x822C1CD0; Hex-Rays
//          renders that compare as `== 0.0`.
//   flt_82001C98 == 1.0f  -- the same function loads it @0x822C1D04 and stores
//          it to mfMaxBoost (+0xA4) @0x822C1D08 as the "max boost came out
//          zero" fallback; Hex-Rays renders that store as `= 1.0`.
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// OnTakedown @ 0x822A6638 -- vtable slot 3.
//
//   0x822A664C  lwz   r11, 0x134(r31)     ; miBoostLevel
//   0x822A6650  cmpwi cr6, r11, 3         ; KI_BOOST_LEVELS
//   0x822A6654  bge   cr6, loc_822A6660
//   0x822A6658  addi  r11, r11, 1
//   0x822A665C  stw   r11, 0x134(r31)
//   0x822A6660  lwz   r11, 0(r31)
//   0x822A6664  li    r4, 0
//   0x822A666C  lwz   r11, 0xC8(r11)      ; slot 50 -> B3::UpdateMaxBoost(false)
//   0x822A6674  bctrl
//   0x822A6678  lwz   r11, 0(r31)
//   0x822A667C  lfs   f1, 0x28(r31)       ; mfTakedownEarning -> f1 (PPC float arg)
//   0x822A6684  lwz   r11, 0xC4(r11)      ; slot 49 -> BoostStrategy::AddBoost
//   0x822A668C  bctrl
//
// `cmpwi` is the SIGNED compare, matching the signed s32 miBoostLevel (the same
// member is sign-extended with `extsw` in UpdateMaxBoost @0x822C1CA4).
//
// CALL ORDER IS LOAD-BEARING: UpdateMaxBoost first, AddBoost second. The sibling
// OnStuntCompletion @0x822A6830 (partfile 04) raises the same level but calls
// them the other way round; do not normalise one against the other.
//
// DIVERGENCES FROM FEB-2007 (asm wins):
//   * Feb-2007 (BrnBoostBurnout3.cpp:117) guards with
//     `if(miBoostLevel<KI_BOOST_LEVELS-1)` and defines KI_BOOST_LEVELS as 5
//     (Feb-2007 BrnBoostBurnout3.h:109). Retail compares against the bare
//     constant and DecFIGS pins KI_BOOST_LEVELS == 3 (DWARF
//     BrnBoostBurnout3.h:134), which is exactly the `cmpwi cr6, r11, 3` above.
//   * Feb-2007's tail (cpp:122-123) is `UpdateMaxBoost(); mfBoostAmount =
//     mfMaxBoost;` -- an instant full bar. Retail neither writes mfBoostAmount (+0xA0) nor
//     mfMaxBoost (+0xA4) here; it earns the tuned takedown reward through the
//     virtual AddBoost, which applies the speed-scaling and earning gates.
// ---------------------------------------------------------------------------
void
BoostBurnout3::OnTakedown()
{
    if( miBoostLevel < KI_BOOST_LEVELS )
    {
        ++miBoostLevel;
    }

    // Unqualified: name lookup finds BoostBurnout3's OWN virtual
    // UpdateMaxBoost(bool) (its vtable slot 50, +0xC8), which HIDES the base's
    // non-virtual one. Qualifying this would call the wrong function.
    UpdateMaxBoost( false );

    AddBoost( mfTakedownEarning );
}

// ---------------------------------------------------------------------------
// OnTakenDownByAIOrPlayer @ 0x822A6930 -- vtable slot 4.
//
//   0x822A6930  lwz   r11, 0x134(r3)      ; miBoostLevel
//   0x822A6934  cmpwi cr6, r11, 1
//   0x822A6938  ble   cr6, loc_822A694C
//   0x822A693C  addi  r11, r11, -1
//   0x822A6940  li    r10, 1
//   0x822A6944  stw   r11, 0x134(r3)
//   0x822A6948  stb   r10, 0xCA(r3)       ; mbJustLostBoostChunk = true
//   0x822A694C  lwz   r11, 0(r3)
//   0x822A6950  li    r4, 0
//   0x822A6954  lwz   r11, 0xC8(r11)      ; slot 50 -> B3::UpdateMaxBoost(false)
//   0x822A695C  bctr                      ; TAIL call
//
// No frame is set up at all -- the whole body is eleven instructions ending in a
// tail dispatch, so `this` stays in r3 throughout.
//
// This is the floor half of the level range: OnTakedown raises while below
// KI_BOOST_LEVELS (3), this lowers while above 1, so retail's live range is
// [1, KI_BOOST_LEVELS]. The `1` is a bare immediate; the DWARF declares no named
// lower-bound constant for it, so it stays a literal rather than an invented one.
//
// mbJustLostBoostChunk (+0xCA) is the latch BoostStrategy::HasJustLostBoostChunk
// @0x822A5EC0 reads-and-clears; setting it INSIDE the guard is what the asm does
// (the `stb` is on the taken-branch side of `ble`), so a takedown that finds the
// level already at 1 raises no chunk-lost notification.
//
// No Feb-2007 counterpart exists -- the method post-dates that drop.
// ---------------------------------------------------------------------------
void
BoostBurnout3::OnTakenDownByAIOrPlayer()
{
    if( miBoostLevel > 1 )
    {
        --miBoostLevel;
        mbJustLostBoostChunk = true;
    }

    UpdateMaxBoost( false );
}

// ---------------------------------------------------------------------------
// OnWrecked @ 0x822C2438 -- vtable slot 8 (r3 = this, r4 = lbIsInOnlineGameMode).
// Fired by BoostStrategy::SetWrecking on the false->true edge (X360: inlined
// into BoostManager::SetWrecking @0x822B8E40).
//
//   0x822C2438  lfs   f0,  0xA0(r3)       ; mfBoostAmount
//   0x822C243C  lis   r11, flt_82001CC0@ha
//   0x822C2440  lfs   f13, 0x94(r3)       ; mfOnWrecked
//   0x822C2444  fsubs f0, f0, f13
//   0x822C2448  stfs  f0,  0xA0(r3)
//   0x822C244C  lfs   f12, flt_82001CC0@l(r11)   ; f12 = 0.0f
//   0x822C2450  fcmpu cr6, f0, f12
//   0x822C2454  bge   cr6, loc_822C245C
//   0x822C2458  stfs  f12, 0xA0(r3)
//        ^-- BoostStrategy::RemoveBoost (DecFIGS BrnBoostStrategy.cpp:168)
//            inlined: subtract, then clamp at zero. Byte-for-byte the same shape
//            the compiler emitted for the same helper in BoostBurnout2::OnWrecked
//            (@0x822C1608..0x822C1620) and BoostBurnout2::OnCrash
//            (@0x822A6370..0x822A6390). Reversing the inline restores the call.
//   loc_822C245C:
//   0x822C245C  lfs    f13, 0xA0(r3)      ; v = mfBoostAmount
//   0x822C2460  clrlwi r11, r4, 24        ; r11 = (u8)lbIsInOnlineGameMode
//   0x822C2464  fneg   f11, f13
//   0x822C2468  lfs    f0,  0xA4(r3)      ; f0 = mfMaxBoost (LIVE to the end)
//   0x822C246C  cmplwi cr6, r11, 0        ; ...the compare, hoisted here
//   0x822C2470  fsel   f13, f11, f12, f13 ; (-v  >= 0) ? 0.0f : v
//   0x822C2474  fsubs  f11, f0,  f13
//   0x822C2478  fsel   f13, f11, f13, f0  ; (max-v >= 0) ? v    : max
//   0x822C247C  stfs   f13, 0xA0(r3)
//   0x822C2480  beqlr  cr6                ; ...and its branch: offline -> return
//   0x822C2484  lfs    f11, 0x100(r3)     ; mfMinBoostAllowedAmount
//   0x822C2488  fcmpu  cr6, f13, f11
//   0x822C248C  bgelr  cr6
//   0x822C2490  lis    r11, flt_82001C98@ha
//   0x822C2494  lfs    f13, flt_82001C98@l(r11)  ; 1.0f
//   0x822C2498  fadds  f13, f11, f13      ; t = mfMinBoostAllowedAmount + 1.0f
//   0x822C249C  fneg   f11, f13
//   0x822C24A0  fsel   f13, f11, f12, f13 ; (-t  >= 0) ? 0.0f : t
//   0x822C24A4  fsubs  f12, f0,  f13
//   0x822C24A8  fsel   f0,  f12, f13, f0  ; (max-t >= 0) ? t    : max
//   0x822C24AC  stfs   f0,  0xA0(r3)
//   0x822C24B0  blr
//
// THE PARAMETER IS LIVE, AND THE BODY IS TWO NESTED CONDITIONS. r4 is narrowed
// to a byte at 0x822C2460 and compared at 0x822C246C, but its branch is the
// `beqlr cr6` five instructions later at 0x822C2480 -- the compiler hoisted the
// compare into the floating-point clamp to fill slots, so a straight read of the
// listing can miss that lbIsInOnlineGameMode is tested at all. It is: everything from
// 0x822C2484 onward runs ONLY in an online mode. The identical
// hoisting appears in BoostBurnout3::UpdateMaxBoost, where `clrlwi r11,r30,24`
// @0x822C1D10 / `cmplwi cr6,r11,0` @0x822C1D1C sit three instructions apart from
// their `beq cr6` @0x822C1D30. DWARF agrees the parameter exists
// (`virtual void OnWrecked(bool);`, BrnBoostBurnout3.h / cpp:445).
//
// NaN POLARITY -- the reason this is NOT rw::math::fpu::Clamp. `fsel frD,frA,frB,frC`
// yields frB when frA >= 0 and frC otherwise, and a NaN frA is NOT >= 0, so:
//   * lower half (0x822C2470, 0x822C24A0): frA = -v, frC = v, so a NaN input
//     falls through UNCHANGED;
//   * upper half (0x822C2478, 0x822C24A8): frA = max - v, frC = mfMaxBoost, so a
//     NaN input comes out as mfMaxBoost -- the bar SELF-HEALS on console.
// A three-argument Clamp written as `(v<lo)?lo:((hi<v)?hi:v)` returns NaN
// unchanged and would make a NaN stick forever, so the upper half must be spelled
// as a NEGATED ORDERED predicate. Same spelling as BoostBurnout2::OnWrecked
// @0x822C15D8 (partfile 05), which clamps the same pair of members.
// (Lower half: `if (v < 0.0f)` differs from the fsel only in the sign of a zero
// result -- the fsel turns -0.0f into +0.0f, the `if` leaves it -0.0f. Every
// other input, NaN included, agrees. The sibling spells it `<`; so does this.)
//
// The `bgelr` at 0x822C248C needs no such rewrite in the other direction: `bge`
// after `fcmpu` is taken on greater-or-equal AND on unordered, so the guarded
// body is reached only on a strictly-ordered less-than -- which is exactly what
// C++ `<` means (false for NaN).
//
// mfMaxBoost is loaded ONCE, at 0x822C2468, and f0 is still that value when the
// second clamp uses it at 0x822C24A4; nothing in between can change +0xA4, so
// naming the member twice below is faithful.
//
// THE `+ 1.0f` IS THE SHARED IMAGE-WIDE 1.0f, NOT A NAMED B3 CONSTANT. It is
// flt_82001C98, the same rodata slot BoostBurnout3::UpdateMaxBoost @0x822C1D04
// loads for its mfMaxBoost fallback. It is specifically NOT
// KF_BOOST_CHUNK_AMOUNT: that constant is flt_82014A18 == 26.666666f (the
// multiplier UpdateMaxBoost applies at @0x822C1CC8), a different slot with a
// different value.
//
// miBoostLevel (+0x134) IS NOT TOUCHED HERE. No instruction in this body
// references +0x134; the level floor of 1 is OnTakenDownByAIOrPlayer's
// (@0x822A6934), not this function's.
//
// No Feb-2007 counterpart exists -- Feb-2007's BoostBurnout3 has no OnWrecked
// (the whole wreck path post-dates that drop).
// ---------------------------------------------------------------------------
void
BoostBurnout3::OnWrecked(
    bool lbIsInOnlineGameMode )
{
    RemoveBoost( mfOnWrecked );

    // Shared [0, mfMaxBoost] clamp -- see the NaN POLARITY note above.
    if( mfBoostAmount < 0.0f )
    {
        mfBoostAmount = 0.0f;
    }
    if( !( mfBoostAmount <= mfMaxBoost ) )
    {
        mfBoostAmount = mfMaxBoost;
    }

    if( lbIsInOnlineGameMode )
    {
        // An instant wreck must not leave the car below the amount the mode
        // guarantees, so top the bar back up to just over that floor.
        if( mfBoostAmount < mfMinBoostAllowedAmount )
        {
            f32 lfRestoredBoost = mfMinBoostAllowedAmount + 1.0f;

            if( lfRestoredBoost < 0.0f )
            {
                lfRestoredBoost = 0.0f;
            }
            if( !( lfRestoredBoost <= mfMaxBoost ) )
            {
                lfRestoredBoost = mfMaxBoost;
            }

            mfBoostAmount = lfRestoredBoost;
        }
    }
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_16.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 16.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::Prepare  @ 0x822C1680   (vtable slot 0)
//
// This is the last function of the wave-P BoostBurnout3 group; it was parked
// only because Attrib::Gen::boostparamsasset had no way to read its attribute
// data area. That is fixed: the generated class now carries the 34 named
// accessors the X360 inlines at this call site, so this body reads the record
// through them and contains no offset arithmetic of its own.
//
// The sibling BoostBurnout3 bodies live in BoostBurnout3_wP_01..07.cpp; member
// layout and vtable slot order come from the wave-P keystone BrnBoostStrategy.h
// and from BrnBoostBurnout3.h. Nothing here re-derives either.
//
// DIVERGENCES FROM THE Feb-2007 SOURCE (rung 1 wins; each one walked in the asm):
//   * Feb-2007's Prepare is `BoostStrategy::Prepare(); miBoostLevel = 0;
//     UpdateMaxBoost(); mfBoostAmount = 0; return true;`. Retail EARLY-OUTS when
//     the base returns false (0x822C16A4 bne / 0x822C16A8 li r3,0), seeds
//     miBoostLevel = 2 rather than 0, additionally clears miOldBoostLevel /
//     mfBoostChunkAmount / mfTimeBoosting, loads the whole 34-parameter
//     boostparamsasset record (Feb-2007 loads no asset at all), and clears
//     mbIsBoostFull.
//   * The UpdateMaxBoost it calls is BoostBurnout3's OWN virtual (bool
//     parameter, vtable slot 50), not Feb-2007's private no-argument helper.
// ============================================================================


namespace BrnWorld
{

// The boostparams asset GUID this strategy resolves its tuning collection from.
// NAME, TYPE and VALUE are all DecFIGS-attested verbatim -- the DWARF dump of
// BrnBoostBurnout3.cpp:43 declares
//   const int64_t KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID = 576011;
// and the X360 stages exactly that value inline at 0x822C16DC/0x822C16E8
// (`lis r3,8` + `ori r3,r3,0xCA0B` == 0x0008CA0B == 576011). int64_t is also
// what ConvertI64ToA takes. BoostBurnout2 uses 576005; each strategy has its own
// collection.
const s64 KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID = 576011;

// ----------------------------------------------------------------------------
// Prepare -- vtable slot 0.  X360 @0x822C1680.
//
// Chain the base Prepare (bailing out if it fails -- Feb-2007 ignores the
// result), seed the Burnout-3 chunk state, then fill the base's 34 shared tuning
// parameters from this strategy's boostparamsasset collection.
// ----------------------------------------------------------------------------
bool BoostBurnout3::Prepare()
{
    // 0x822C1698-16AC: `bl BrnWorld__BoostStrategy__Prepare / clrlwi r11,r3,24 /
    // cmplwi cr6,r11,0 / bne`; the fall-through arm is `li r3,0; b <epilogue>`.
    if (!BoostStrategy::Prepare())
    {
        return false;
    }

    // 0x822C16B4/16C8: `li r10,2` / `stw r10,0x134(r31)`. The header pins
    // KI_BOOST_LEVELS == 3 (OnTakedown @0x822A6650 `cmpwi cr6,r11,3`), so the
    // original may well have written `KI_BOOST_LEVELS - 1`; the constant is
    // folded in the image and the spelling is not recoverable, so the literal
    // the binary carries is what is written here.
    miBoostLevel = 2;
    miOldBoostLevel = 0;        // 0x822C16CC: stw r30(=0), 0x138(r31)

    // 0x822C16B0/16BC-16D4: `lwz r11,0(r31) / li r4,0 / lwz r11,0xC8(r11) /
    // mtctr / bctrl` -- vtable slot 50 (0xC8/4), BoostBurnout3's OWN virtual
    // UpdateMaxBoost(bool). It runs BEFORE the three zero stores below.
    UpdateMaxBoost(false);

    // 0x822C16EC-16F8: one `lfs f0, flt_82001CC0` (0.0f) stored three times.
    mfBoostAmount = 0.0f;       // stfs f0, 0xA0(r31)
    mfBoostChunkAmount = 0.0f;  // stfs f0, 0x130(r31)
    mfTimeBoosting = 0.0f;      // stfs f0, 0x13C(r31)

    // 0x822C16DC-1714: print the asset GUID as decimal text (`li r5,0xA` is the
    // radix), hash the text into an attribsys key, and construct the
    // boostparamsasset over it.
    //
    // The key is LIVE, not dead: boostparamsasset's ctor @0x822B8C88 never
    // WRITES r4, so the value handed in here arrives whole at FindCollection and
    // is consumed there as the COLLECTION key (derivation in boostparamsasset.h).
    // Attrib::StringToKey returns the full 64-bit hash and the ctor parameter is
    // u64, so nothing is narrowed -- a static_cast<u32> here would discard the
    // high word and make every collection lookup miss.
    //
    // Buffer size: the console frame places this buffer at +0x70 and the first
    // saved register (var_18, `std r30`) at +0x278, i.e. a 0x208-byte gap == 512
    // plus the frame's 8 bytes of alignment slack. 512 is also the DWARF-attested
    // size of the identical ConvertI64ToA scratch buffer in the sibling
    // CgsAttribSys::AttribSysCollectionKey::GetHashKey (DWARF cpp:81).
    char lacGuidText[512];
    rw::core::stdc::ConvertI64ToA(KI_DEFAULT_AGGRESSION_BOOST_PARAMS_GUID, lacGuidText, 10);

    // 0x822C1708-1714: `mr r4,r3` (the whole doubleword, no clrldi), `li r5,0`
    // for the owner, then the ctor.
    Attrib::Gen::boostparamsasset lBoostParams(Attrib::StringToKey(lacGuidText), nullptr);

    // 0x822C1718-1858 -- the 34 record -> member stores, in ascending member
    // order (which is the order the X360 emits them). Each one goes through the
    // generated accessor; the record offsets live in boostparamsasset.h.
    mfNearMissBoostEarning    = lBoostParams.NearMissBoostEarning();
    mfDriftEarning            = lBoostParams.DriftEarning();
    mfAirEarning              = lBoostParams.AirEarning();
    // The only two Int32 attributes in the record, and the only two stores the
    // X360 converts (lwz + extsw + std/lfd + fcfid + frsp, 0x822C1734-176C).
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
    // BoostBurnout3 KEEPS the three stunt earnings the asset ships; BoostBurnout5
    // re-zeroes them immediately after its own load (@0x822C1F8C).
    mfStuntJumpEarning        = lBoostParams.StuntJumpEarning();
    mfStuntSmashEarning       = lBoostParams.StuntSmashEarning();
    mfStuntBillBoardEarning   = lBoostParams.StuntBillBoardEarning();
    mfCrashEscapeBoostEarning = lBoostParams.CrashEscapeBoostEarning();
    mfBoostChainBonus         = lBoostParams.BoostChainBonus();
    mfOnWrecked               = lBoostParams.OnWrecked();

    // 0x822C1854: `stb r30(=0), 0xC3(r31)`. The scheduler slotted it between the
    // last two parameter stores; it is the only non-parameter store in the tail.
    mbIsBoostFull = false;

    // 0x822C185C: `bl Attrib::Instance::~Instance` -- lBoostParams leaving scope.
    return true;                // 0x822C1860: li r3,1
}

} // namespace BrnWorld

// ============================================================================
// FOLDED FROM BoostBurnout3_wP_17.cpp (wave P) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 17.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::UpdateStuntBoost @ 0x822A6708   (base vtable slot 48, +0xC0)
//
// This body was parked in wave P because
// BrnGameState::GameStateModuleIO::CompletedStuntAction was only FORWARD-declared
// in the tree (BrnBoostStrategy.h:87). It now has a real definition in its home,
// GameSource/GameState/BrnGameActions.h:766, so the four payload reads below name
// real members. Nothing is forked here.
// ============================================================================


namespace BrnWorld
{

// ---------------------------------------------------------------------------
// UpdateStuntBoost @ 0x822A6708 -- the base's vtable slot 48 (+0xC0), overridden.
//
// Pays out the four one-shot stunt awards carried by a CompletedStuntAction, each
// gated on its own bit of the action's muStuntActionComplete bitfield, each paid
// through the base's protected virtual AddBoost (slot 49, +0xC4).
//
// Full asm (re-derived from the export at 0x822A6708; r31 == this, r30 == the
// action pointer, set up by `mr r30, r4` / `mr r31, r3` @0x822A671C-20):
//
//   0x822A6724  cmplwi cr6, r30, 0                 ; lpCompletedStuntAction == NULL?
//   0x822A6728  bne    cr6, loc_822A674C
//   0x822A672C  bl     CgsDev::Assert::BeginAssert
//   0x822A6734  li     r5, 0x14A                   ; line 330
//   0x822A6738  addi   r4, r11, aDP4B5MainBurno_531 ; "d:\p4\b5_main\burnout\main\code\g"...
//   0x822A6740  addi   r3, r11, aLpcompletedstu_0  ; "lpCompletedStuntAction != NULL"
//   0x822A6744  bl     CgsDev::Assert::FireAssert
//   0x822A6748  bl     CgsDev::Assert::EndAssert
// loc_822A674C:                                    ; NOTE: NO early-out. The assert
//                                                  ; falls straight through and the
//                                                  ; code dereferences r30 anyway --
//                                                  ; CGS_ASSERT is report-and-continue
//                                                  ; on this build, so a plain
//                                                  ; CGS_ASSERT with no `return`
//                                                  ; reproduces it exactly.
//   0x822A674C  lwz    r11, 0(r30)                 ; muStuntActionComplete
//   0x822A6750  clrlwi r11, r11, 31                ; & 0x1  -> BARREL_ROLL
//   0x822A6758  beq    cr6, loc_822A6790
//   0x822A675C  lwz    r11, 0x18(r30)              ; miCompletedBarrelRolls
//   0x822A6760  lfs    f0,  0x5C(r31)              ; mfBarrelRollEarning
//   0x822A6764  lwz    r10, 0(r31)                 ; vptr
//   0x822A676C  extsw  r11, r11                    ; SIGNED 32-bit -> s32, not u32
//   0x822A6770  lwz    r10, 0xC4(r10)              ; +0xC4 == slot 49 == AddBoost
//   0x822A677C  fcfid  f13, f13                    ; int -> f64
//   0x822A6780  frsp   f13, f13                    ; -> f32
//   0x822A6784  fmuls  f1,  f13, f0                ; count * mfBarrelRollEarning
//   0x822A678C  bctrl
// loc_822A6790:
//   0x822A6790  lwz    r11, 0(r30)                 ; RE-READ (see the re-read note)
//   0x822A6794  rlwinm r11, r11, 0,30,30           ; & 0x2  -> AIR_SPIN
//   0x822A679C  beq    cr6, loc_822A67C0
//   0x822A67A4  lfs    f0,  0x58(r31)              ; mfAirSpinEarning
//   0x822A67A8  lfs    f13, 8(r30)                 ; mfCompletedAirSpinAngle
//   0x822A67B0  fmuls  f1,  f0, f13                ; earning * angle (operand order)
//   0x822A67B4  lwz    r11, 0xC4(r11)              ; AddBoost
//   0x822A67BC  bctrl
// loc_822A67C0:
//   0x822A67C0  lwz    r11, 0(r30)
//   0x822A67C4  rlwinm r11, r11, 0,29,29           ; & 0x4  -> HANDBREAK_TURN
//   0x822A67CC  beq    cr6, loc_822A67F0
//   0x822A67D4  lfs    f0,  0x50(r31)              ; mfHandbrake180Earning
//   0x822A67D8  lfs    f13, 0xC(r30)               ; mfCompletedHandbreakTurnAngle
//   0x822A67E0  fmuls  f1,  f0, f13
//   0x822A67EC  bctrl                              ; AddBoost
// loc_822A67F0:
//   0x822A67F0  lwz    r11, 0(r30)
//   0x822A67F4  rlwinm r11, r11, 0,28,28           ; & 0x8  -> CLEANLANDING
//   0x822A67FC  beq    cr6, loc_822A6818
//   0x822A6804  lfs    f1,  0x60(r31)              ; mfCleanLanding -- FLAT award,
//                                                  ; reads no payload and applies no
//                                                  ; scale (there is no fmuls here)
//   0x822A6814  bctrl                              ; AddBoost
// loc_822A6818: epilogue.
//
// SIGNEDNESS. The barrel-roll payload is `lwz` + `extsw` + `fcfid` @0x822A675C /
// 0x822A676C / 0x822A677C -- a SIGN-EXTENDED word converted by the signed
// int-to-float instruction, i.e. an s32. That is what pins CompletedStuntAction's
// +0x18 as `s32 miCompletedBarrelRolls` (BrnGameActions.h:774) and it is why the
// cast below is on a signed member. `fcfid` on a zero-extended word would be the
// unsigned form; it is not what the compiler emitted.
//
// THE RE-READ IS REAL, NOT AN ARTEFACT. muStuntActionComplete is re-loaded from
// 0(r30) before EACH of the four tests (0x822A674C / 0x822A6790 / 0x822A67C0 /
// 0x822A67F0) rather than being held in a register across the AddBoost calls.
// Reading the member expression fresh in each `if` below reproduces that; hoisting
// it into a local would not. (AddBoost is an out-of-line virtual and the action is
// a `const` pointer to non-const-qualified storage, so the compiler had to reload.)
//
// THE FOUR BITS. The masks are the physics detector's completed-stunt bitfield,
// whose DWARF-verbatim enumerators already have a home in
// GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h
// (`BrnPhysics::EStuntActionComplete`, values 1/2/4/8 for BARREL_ROLL / AIR_SPIN /
// HANDBREAK_TURN / CLEANLANDING). Those are pre-existing in-tree declarations, NOT
// minted here; what the asm attests on its own is only bit positions 0/1/2/3 of the
// u32. The binding is corroborated by which earning member each bit pays:
// bit0 -> mfBarrelRollEarning (+0x5C), bit1 -> mfAirSpinEarning (+0x58),
// bit2 -> mfHandbrake180Earning (+0x50), bit3 -> mfCleanLanding (+0x60) -- the four
// names line up one-for-one with the four enumerators, in the same bit order.
// (Offsets from BrnBoostStrategy.h:453/455/456/457.)
//
// OPERAND ORDER. Kept as emitted: the barrel-roll multiply is `fmuls f1, f13, f0`
// (count * earning) while the air-spin and handbrake ones are `fmuls f1, f0, f13`
// (earning * angle). Float multiply is not associative-safe to reorder in general,
// and the differing order is itself evidence of how the two source expressions were
// written; there is no reason to normalise it.
//
// SIBLINGS. BoostBurnout2 @0x822A6478 and BoostBurnout5 @0x822A6F98 are the same
// body -- all three are 74 instructions and differ only in the two assert operands,
// which are per-file by construction:
//   B3 (here) `li r5, 0x14A` = line 330, file string aDP4B5MainBurno_531
//   B2        `li r5, 0x304` = line 772, file string aDP4B5MainBurno_530
//   B5        `li r5, 0x33E` = line 830, file string aDP4B5MainBurno_532
// The expression string aLpcompletedstu_0 ("lpCompletedStuntAction != NULL") is the
// SAME pooled literal in all three. So the three overrides share this source text.
//
// FEB-2007 has no UpdateStuntBoost at all -- the whole slot-48 stunt-boost path
// postdates it -- so there is nothing to reconcile against; this body is purely
// asm-derived.
// ---------------------------------------------------------------------------
void BoostBurnout3::UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction)
{
    CGS_ASSERT(lpCompletedStuntAction != 0, "lpCompletedStuntAction != NULL");

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_BARREL_ROLL) != 0 )
    {
        AddBoost(static_cast<f32>(lpCompletedStuntAction->miCompletedBarrelRolls) * mfBarrelRollEarning);
    }

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_AIR_SPIN) != 0 )
    {
        AddBoost(mfAirSpinEarning * lpCompletedStuntAction->mfCompletedAirSpinAngle);
    }

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_HANDBREAK_TURN) != 0 )
    {
        AddBoost(mfHandbrake180Earning * lpCompletedStuntAction->mfCompletedHandbreakTurnAngle);
    }

    if( (lpCompletedStuntAction->muStuntActionComplete & BrnPhysics::E_STUNT_ACTION_COMPLETE_CLEANLANDING) != 0 )
    {
        AddBoost(mfCleanLanding);
    }
}

}
