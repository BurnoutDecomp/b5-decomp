#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT_AT
#include "GameSource/Network/Utilities/BrnNetworkRounder.h"      // NetworkRounder::RoundFloatToAccuracy

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
        // This build has two more scoring skills (9 and 10, names unrecovered) ahead of the
        // unused slot 11: the skillz message sends skills 0..10, the skills menu never folds
        // in slot 11, and it treats 12 / 13 as the road-rule time / crash records.
        // FLAG: E_BURNOUT_SKILL_ROAD_RULE_TIME / _CRASH below keep the older values (10 / 11);
        // this build's road-rule slots are E_BURNOUT_SKILL_EXTRA_12 / _13.
        E_BURNOUT_SKILL_TO_SEND_VIA_NETWORK_COUNT = 11,
        E_BURNOUT_SKILL_TOTAL                   = 11,
        E_BURNOUT_SKILL_ROAD_RULE_TIME          = 10,
        E_BURNOUT_SKILL_ROAD_RULE_CRASH         = 11,
        // GROWN (FLAG): the X360 build (Clear @0x8230FBF0 loop bound, GetSkillAccuracy
        // @0x8230FCD8 + SetBurnoutSkill @0x8231C9A8 range guards) all use E_BURNOUT_SKILL_COUNT
        // == 14, two more than the LegacySrc PS3 DWARF's 12. The asm OVERRIDES DWARF, so the
        // count grows to 14 and the accumulator array to [14]. The DWARF did not name these two
        // extra skill slots; they are added additively as placeholders (existing enumerators are
        // left in place, never reordered/retyped). If the reference later names them, rename here.
        E_BURNOUT_SKILL_EXTRA_12                = 12,
        E_BURNOUT_SKILL_EXTRA_13                = 13,
        E_BURNOUT_SKILL_COUNT                   = 14,
    };

    // Clear lives in this type's own TU; the accessors are header-inline below (their asserts name
    // this header in ARTIST). DecFIGS describes the parameter and scalar types.
    // (float32_t -> f32 per project scalar types).
    void Clear();                                              // BrnBurnoutSkillzData.h:85
    f32  GetBurnoutSkill(EBurnoutSkillType eSkill) const;      // BrnBurnoutSkillzData.h:90
    void SetBurnoutSkill(EBurnoutSkillType eSkill, f32 fVal);  // BrnBurnoutSkillzData.h:96
    // ARTIST 0x8231CA10 passes the skill in r3 without a this pointer.
    static f32 GetSkillAccuracy(EBurnoutSkillType eSkill);           // BrnBurnoutSkillzData.h:101

private:
    // GROWN (FLAG): array sized to E_BURNOUT_SKILL_COUNT == 14 to match the X360 asm.
    // Clear (0x8230FBF0) zeroes 14 contiguous f32 accumulators; SetBurnoutSkill (0x8231C9A8)
    // stores at &mafBurnoutSkilz[leSkill] for leSkill in [0,14). The LegacySrc PS3 DWARF said
    // [12]; the X360 asm is authoritative for the project, so this grows additively to [14].
    // Growing this inner array also widens the by-value embedder ScoringSystem::maBurnoutSkillzData[8];
    // that is a SEMANTIC slice (named members + order/types), not a byte-exact sizeof, so the
    // grow is in-policy. FLAG for the consolidator.
    f32 mafBurnoutSkilz[14]; // BrnBurnoutSkillzData.h:104 (EBurnoutSkillType count == 14, X360)
};

// Postfix increment used to iterate EBurnoutSkillType (BrnBurnoutSkillzData.h:107).
// Declare-only -- body lives in the BurnoutSkillzData TU.
BurnoutSkillzData::EBurnoutSkillType operator++(BurnoutSkillzData::EBurnoutSkillType& eSkill, int);

// Header-inline accessors (DWARF h:90/:96/:101; ARTIST asserts at h:135/:147/:161, inlined into
// BrnNetwork::BurnoutSkillzMessage::PackOrUnpack 0x8257F338, out-of-line copies 0x8230FC60 /
// 0x8231C9A8 / 0x8230FCD8).
#define CGS_ARTIST_BRNBURNOUTSKILLZDATA_H "..\\..\\..\\GameSource\\GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h"
inline f32 BurnoutSkillzData::GetBurnoutSkill(EBurnoutSkillType eSkill) const
{
    CGS_ASSERT_AT(eSkill >= E_BURNOUT_SKILL_START,
        "leSkillType >= E_BURNOUT_SKILL_START", CGS_ARTIST_BRNBURNOUTSKILLZDATA_H, 135 /* ARTIST 0x8230FC8C */);

    CGS_ASSERT_AT(eSkill < E_BURNOUT_SKILL_COUNT,
        "leSkillType < E_BURNOUT_SKILL_COUNT", CGS_ARTIST_BRNBURNOUTSKILLZDATA_H, 136 /* ARTIST 0x8230FCB0 */);


    return mafBurnoutSkilz[eSkill];
}

// Store one skill sample, snapped to the skill's rounding accuracy (ARTIST 0x8231CA18 rounds the
// incoming sample in its parameter slot).
inline void BurnoutSkillzData::SetBurnoutSkill(EBurnoutSkillType eSkill, f32 lfVal)
{
    CGS_ASSERT_AT(eSkill >= E_BURNOUT_SKILL_START, "leSkillType >= E_BURNOUT_SKILL_START", CGS_ARTIST_BRNBURNOUTSKILLZDATA_H, 147);
    CGS_ASSERT_AT(eSkill < E_BURNOUT_SKILL_COUNT, "leSkillType < E_BURNOUT_SKILL_COUNT", CGS_ARTIST_BRNBURNOUTSKILLZDATA_H, 148);

    const f32 lfAccuracy = GetSkillAccuracy(eSkill);

    BrnNetwork::NetworkRounder::RoundFloatToAccuracy(&lfVal, lfAccuracy);

    mafBurnoutSkilz[eSkill] = lfVal;
}

// The fixed rounding accuracy of a skill sample (ARTIST 0x8230FD44 loads 0.005 with lfs).
inline f32 BurnoutSkillzData::GetSkillAccuracy(EBurnoutSkillType eSkill)
{
    CGS_ASSERT_AT(eSkill >= E_BURNOUT_SKILL_START, "leSkillType >= E_BURNOUT_SKILL_START", CGS_ARTIST_BRNBURNOUTSKILLZDATA_H, 161);
    CGS_ASSERT_AT(eSkill < E_BURNOUT_SKILL_COUNT, "leSkillType < E_BURNOUT_SKILL_COUNT", CGS_ARTIST_BRNBURNOUTSKILLZDATA_H, 162);

    return 0.005f;
}
#undef CGS_ARTIST_BRNBURNOUTSKILLZDATA_H
}
