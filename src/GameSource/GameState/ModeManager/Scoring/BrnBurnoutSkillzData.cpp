// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.cpp
// ============================================================================
// Out-of-line method BODIES for BrnGameState::BurnoutSkillzData -- the per-car
// "burnout skillz" tally (one f32 accumulator per EBurnoutSkillType) that
// ScoringSystem embeds BY VALUE as maBurnoutSkillzData[8]. The layout + every
// signature are owned by the home BrnBurnoutSkillzData.h; this TU only bodies
// the three addressed methods.
//
// SHAPE = the home header's layout; BODY = the console build (which overrides the
// declared shape on conflict). Members accessed BY NAME against the home array -- no
// offset casts.
//
//   Clear             zero all E_BURNOUT_SKILL_COUNT accumulators
//   (GetBurnoutSkill / SetBurnoutSkill / GetSkillAccuracy are header-inline: the home header.)
//
// LAYOUT FLAG (see the home header): the console range-guards and loops to
// E_BURNOUT_SKILL_COUNT == 14, two more than the declared 12. The home was grown
// additively (array -> [14], two placeholder enumerators) to match the console.
// ----------------------------------------------------------------------------
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h" // home (layout + decls)


// CGS_ASSERT (the house assert machinery the console Begin/Fire/End triples map to).
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // BurnoutSkillzData::operator++  (postfix, iterates EBurnoutSkillType)
    // ------------------------------------------------------------------------
    // Declared in the home header. Clear walks the enum with this post-increment; the
    // console Clear body inlines its bounds assert (leEnumIndex <= E_BURNOUT_SKILL_COUNT)
    // into the loop -- reproduced here so the assert fires exactly where it does on the
    // console.
    BurnoutSkillzData::EBurnoutSkillType operator++(BurnoutSkillzData::EBurnoutSkillType& eSkill, int)
    {
        const BurnoutSkillzData::EBurnoutSkillType eOld = eSkill;
        eSkill = static_cast<BurnoutSkillzData::EBurnoutSkillType>(static_cast<int>(eSkill) + 1);
        CGS_ASSERT_AT(static_cast<int>(eSkill) <= BurnoutSkillzData::E_BURNOUT_SKILL_COUNT,
            "leEnumIndex <= BurnoutSkillzData::E_BURNOUT_SKILL_COUNT",
                CGS_ARTIST_FILE, 109 /* ARTIST 0x8230FC38 */);

        return eOld;
    }

    // ------------------------------------------------------------------------
    // BurnoutSkillzData::Clear
    // ------------------------------------------------------------------------
    // Zero every skill accumulator. The console stores one f32 at a time, walking the
    // array for E_BURNOUT_SKILL_COUNT (== 14) elements and firing the iterating enum's
    // bounds assert each step. Expressed by name as the indexed clear over
    // mafBurnoutSkilz, driven by the post-increment iterator so the per-step assert is
    // preserved. (The console's calling convention hands back the array base; the home
    // declares void Clear -- that returned pointer is a convention artifact no caller
    // uses.)
    void BurnoutSkillzData::Clear()
    {
        for (EBurnoutSkillType eSkill = E_BURNOUT_SKILL_START;
             eSkill < E_BURNOUT_SKILL_COUNT;
             eSkill++)
        {
            mafBurnoutSkilz[eSkill] = 0.0f;
        }
    }

}

#undef CGS_ARTIST_FILE
