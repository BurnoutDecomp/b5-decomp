#pragma once

#include "types.hpp"

// Minimal-slice home for BrnGameState::BurnoutSkillzData -- the per-event "burnout skillz"
// tally that BrnScoringSystem embeds BY VALUE and calls into. This is a complete, compilable
// placeholder: the DWARF (DecFIGS GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h)
// spells the full shape out, so this slice models it exactly --
//   - base-less struct in namespace BrnGameState (DWARF shows no base),
//   - the nested EBurnoutSkillType enum (BrnBurnoutSkillzData.h:48),
//   - the single private member f32 mafBurnoutSkilz[12] (BrnBurnoutSkillzData.h:104; the whole
//     type is this one 48-byte array -- no NOMINAL reserved storage needed),
//   - declare-only public methods (BrnBurnoutSkillzData.h:85-101).
// Method BODIES (and any future growth) belong to this type's own TU -- grow this home in place,
// do NOT fork. Project scalar f32 used in place of the DWARF's float32_t.
namespace BrnGameState
{
struct BurnoutSkillzData
{
    // BrnBurnoutSkillzData.h:48
    enum EBurnoutSkillType
    {
        E_BURNOUT_SKILL_START                   = 0,
        E_BURNOUT_SKILL_AIR_TIME                = 0,
        E_BURNOUT_SKILL_BARREL_ROLL             = 1,
        E_BURNOUT_SKILL_BOOST_CHAIN             = 2,
        E_BURNOUT_SKILL_DRIFT                   = 3,
        E_BURNOUT_SKILL_SPIN                    = 4,
        E_BURNOUT_SKILL_AIR_DISTANCE            = 5,
        E_BURNOUT_SKILL_NEAR_MISSES             = 6,
        E_BURNOUT_SKILL_ONCOMING                = 7,
        E_BURNOUT_SKILL_POWER_PARKING           = 8,
        E_BURNOUT_SKILL_TO_SEND_VIA_NETWORK_COUNT = 9,
        E_BURNOUT_SKILL_TOTAL                   = 9,
        E_BURNOUT_SKILL_ROAD_RULE_TIME          = 10,
        E_BURNOUT_SKILL_ROAD_RULE_CRASH         = 11,
        E_BURNOUT_SKILL_COUNT                   = 12,
    };

    // Declare-only -- bodies live in this type's own TU. Signatures verbatim from DWARF
    // (float32_t -> f32 per project scalar types).
    void Clear();                                              // BrnBurnoutSkillzData.h:85
    f32  GetBurnoutSkill(EBurnoutSkillType eSkill) const;      // BrnBurnoutSkillzData.h:90
    void SetBurnoutSkill(EBurnoutSkillType eSkill, f32 fVal);  // BrnBurnoutSkillzData.h:96
    f32  GetSkillAccuracy(EBurnoutSkillType eSkill);           // BrnBurnoutSkillzData.h:101

private:
    f32 mafBurnoutSkilz[12]; // BrnBurnoutSkillzData.h:104 (EBurnoutSkillType count == 12)
};

// Postfix increment used to iterate EBurnoutSkillType (BrnBurnoutSkillzData.h:107).
// Declare-only -- body lives in the BurnoutSkillzData TU.
BurnoutSkillzData::EBurnoutSkillType operator++(BurnoutSkillzData::EBurnoutSkillType& eSkill, int);
}
