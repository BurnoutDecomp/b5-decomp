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

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"

// EImpactType is only forward-declared by BrnBoostStrategy.h (`enum
// EImpactType : s32;`); the enumerator VALUES are needed here, so pull in the
// type's home header. StuntElementType's home (BrnGameStateTypes.h) is already
// included transitively by BrnBoostStrategy.h.
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"

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
