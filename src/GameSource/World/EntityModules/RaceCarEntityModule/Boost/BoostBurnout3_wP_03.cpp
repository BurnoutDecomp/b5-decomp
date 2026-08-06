#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"  // BrnPhysics::Vehicle::EImpactType enumerators

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
