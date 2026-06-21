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
        // GROWN (FLAG): the X360 build (Clear @0x8230FBF0 loop bound, GetSkillAccuracy
        // @0x8230FCD8 + SetBurnoutSkill @0x8231C9A8 range guards) all use E_BURNOUT_SKILL_COUNT
        // == 14, two more than the Feb-2007 PS3 DWARF's 12. The asm OVERRIDES DWARF, so the
        // count grows to 14 and the accumulator array to [14]. The DWARF did not name these two
        // extra skill slots; they are added additively as placeholders (existing enumerators are
        // left in place, never reordered/retyped). If the leak later names them, rename here.
        E_BURNOUT_SKILL_EXTRA_12                = 12,
        E_BURNOUT_SKILL_EXTRA_13                = 13,
        E_BURNOUT_SKILL_COUNT                   = 14,
    };

    // Declare-only -- bodies live in this type's own TU. Signatures verbatim from DWARF
    // (float32_t -> f32 per project scalar types).
    void Clear();                                              // BrnBurnoutSkillzData.h:85
    f32  GetBurnoutSkill(EBurnoutSkillType eSkill) const;      // BrnBurnoutSkillzData.h:90
    void SetBurnoutSkill(EBurnoutSkillType eSkill, f32 fVal);  // BrnBurnoutSkillzData.h:96
    f32  GetSkillAccuracy(EBurnoutSkillType eSkill);           // BrnBurnoutSkillzData.h:101

private:
    // GROWN (FLAG): array sized to E_BURNOUT_SKILL_COUNT == 14 to match the X360 asm.
    // Clear (0x8230FBF0) zeroes 14 contiguous f32 accumulators; SetBurnoutSkill (0x8231C9A8)
    // stores at &mafBurnoutSkilz[leSkill] for leSkill in [0,14). The Feb-2007 PS3 DWARF said
    // [12]; the X360 asm is authoritative for the project, so this grows additively to [14].
    // Growing this inner array also widens the by-value embedder ScoringSystem::maBurnoutSkillzData[8];
    // that is a SEMANTIC slice (named members + order/types), not a byte-exact sizeof, so the
    // grow is in-policy. FLAG for the consolidator.
    f32 mafBurnoutSkilz[14]; // BrnBurnoutSkillzData.h:104 (EBurnoutSkillType count == 14, X360)
};

// Postfix increment used to iterate EBurnoutSkillType (BrnBurnoutSkillzData.h:107).
// Declare-only -- body lives in the BurnoutSkillzData TU.
BurnoutSkillzData::EBurnoutSkillType operator++(BurnoutSkillzData::EBurnoutSkillType& eSkill, int);
}
