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
//   GetSkillAccuracy  per-skill rounding accuracy (a constant 0.005)
//   SetBurnoutSkill   round one sample to the skill accuracy and store it
//
// LAYOUT FLAG (see the home header): the console range-guards and loops to
// E_BURNOUT_SKILL_COUNT == 14, two more than the declared 12. The home was grown
// additively (array -> [14], two placeholder enumerators) to match the console.
// ----------------------------------------------------------------------------

#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h" // home (layout + decls)

// BrnNetwork::NetworkRounder::RoundFloatToAccuracy -- SetBurnoutSkill snaps the sample
// to the per-skill accuracy through this helper.
#include "GameSource/Network/Utilities/BrnNetworkRounder.h"

// CGS_ASSERT (the house assert machinery the console Begin/Fire/End triples map to).
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
        CGS_ASSERT(static_cast<int>(eSkill) <= BurnoutSkillzData::E_BURNOUT_SKILL_COUNT,
                   "leEnumIndex <= BurnoutSkillzData::E_BURNOUT_SKILL_COUNT");
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

    // ------------------------------------------------------------------------
    // BurnoutSkillzData::GetSkillAccuracy
    // ------------------------------------------------------------------------
    // Return the rounding accuracy used for one skill sample. Two leading range
    // guards (eSkill >= E_BURNOUT_SKILL_START; eSkill < E_BURNOUT_SKILL_COUNT), then
    // return the fixed accuracy constant the console loads (0.0049999999). The
    // "cell/ppu" phantom dep is this libm-style constant return; no math call is
    // actually performed, so a plain literal matches the console.
    //
    // RETURN-TYPE FLAG: the console's calling convention and the declared shape both
    // make this a `double`, while the home commits the return as `f32`. Bodied against
    // the committed `f32` here -- the constant the console loads is single-precision
    // either way; FLAGGED for the consolidator rather than retyping the committed home.
    f32 BurnoutSkillzData::GetSkillAccuracy(EBurnoutSkillType eSkill)
    {
        CGS_ASSERT(eSkill >= E_BURNOUT_SKILL_START, "leSkillType >= E_BURNOUT_SKILL_START");
        CGS_ASSERT(eSkill < E_BURNOUT_SKILL_COUNT, "leSkillType < E_BURNOUT_SKILL_COUNT");

        return 0.0049999999f;
    }

    // ------------------------------------------------------------------------
    // BurnoutSkillzData::GetBurnoutSkill
    // ------------------------------------------------------------------------
    // Read one skill sample back. The console body is the two range guards (eSkill in
    // [E_BURNOUT_SKILL_START, E_BURNOUT_SKILL_COUNT), reported against the home header)
    // followed by a single indexed float load from mafBurnoutSkilz -- the array base IS
    // the object base, so the load sits at +0x00 plus four bytes per skill.
    f32 BurnoutSkillzData::GetBurnoutSkill(EBurnoutSkillType eSkill) const
    {
        CGS_ASSERT(eSkill >= E_BURNOUT_SKILL_START, "leSkillType >= E_BURNOUT_SKILL_START");
        CGS_ASSERT(eSkill < E_BURNOUT_SKILL_COUNT, "leSkillType < E_BURNOUT_SKILL_COUNT");

        return mafBurnoutSkilz[eSkill];
    }

    // ------------------------------------------------------------------------
    // BurnoutSkillzData::SetBurnoutSkill
    // ------------------------------------------------------------------------
    // Store one skill sample, snapped to the skill's rounding accuracy. Two leading
    // range guards (eSkill in [E_BURNOUT_SKILL_START, E_BURNOUT_SKILL_COUNT)), then:
    //   * GetSkillAccuracy(eSkill) is called for the per-skill accuracy (it also
    //     re-fires the range asserts). The result feeds the rounder's accuracy.
    //   * RoundFloatToAccuracy(&lfVal, accuracy) snaps the sample in place; the console
    //     hands the rounder the stack slot holding the incoming sample.
    //   * the rounded sample is stored at mafBurnoutSkilz[eSkill].
    //
    // ARG-MAPPING NOTE: the console body takes the object, the skill index and the
    // sample -- three arguments and nothing else. The decompiler mints a large garbage
    // argument list (a3..a23 + a24) from dropped and uninitialised scratch; none of it
    // is a real parameter. The sole caller
    // (BrnGameState::BurnoutSkillzManager::SetNewSkillIfGreater) sets up exactly those
    // three, confirming the home's (EBurnoutSkillType eSkill, f32 fVal) signature.
    void BurnoutSkillzData::SetBurnoutSkill(EBurnoutSkillType eSkill, f32 fVal)
    {
        CGS_ASSERT(eSkill >= E_BURNOUT_SKILL_START, "leSkillType >= E_BURNOUT_SKILL_START");
        CGS_ASSERT(eSkill < E_BURNOUT_SKILL_COUNT, "leSkillType < E_BURNOUT_SKILL_COUNT");

        const f32 lfAccuracy = GetSkillAccuracy(eSkill);

        f32 lfVal = fVal;
        BrnNetwork::NetworkRounder::RoundFloatToAccuracy(&lfVal, lfAccuracy);

        mafBurnoutSkilz[eSkill] = lfVal;
    }
}
