// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.cpp
// ============================================================================
// Out-of-line method BODIES for BrnGameState::BurnoutSkillzData -- the per-car
// "burnout skillz" tally (one f32 accumulator per EBurnoutSkillType) that
// ScoringSystem embeds BY VALUE as maBurnoutSkillzData[8]. The layout + every
// signature are owned by the home BrnBurnoutSkillzData.h; this TU only bodies
// the three addressed methods.
//
// SHAPE = home (DWARF-derived) layout; BODY = X360 pseudocode/asm (overrides DWARF
// on conflict). Members accessed BY NAME against the home array -- no offset casts.
//
//   Clear            0x8230FBF0  zero all E_BURNOUT_SKILL_COUNT accumulators
//   GetSkillAccuracy 0x8230FCD8  per-skill rounding accuracy (a constant 0.005)
//   SetBurnoutSkill  0x8231C9A8  round one sample to the skill accuracy and store it
//
// LAYOUT FLAG (see the home header): the X360 asm range-guards / loops to
// E_BURNOUT_SKILL_COUNT == 14, two more than the Feb-2007 PS3 DWARF's 12. The home
// was grown additively (array -> [14], two placeholder enumerators) to match the asm.
// ----------------------------------------------------------------------------

#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h" // home (layout + decls)

// BrnNetwork::NetworkRounder::RoundFloatToAccuracy -- SetBurnoutSkill snaps the sample
// to the per-skill accuracy through this helper (X360 0x8231CA1C bl).
#include "GameSource/Network/Utilities/BrnNetworkRounder.h"

// CGS_ASSERT (the house assert machinery the X360 Begin/Fire/End triples map to).
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // BurnoutSkillzData::operator++  (postfix, iterates EBurnoutSkillType)
    // ------------------------------------------------------------------------
    // Declared in the home (BrnBurnoutSkillzData.h:107). Clear walks the enum with
    // this post-increment; the X360 Clear body inlines its bounds assert
    // (leEnumIndex <= E_BURNOUT_SKILL_COUNT) into the loop -- reproduced here so the
    // assert fires exactly where the asm does.
    BurnoutSkillzData::EBurnoutSkillType operator++(BurnoutSkillzData::EBurnoutSkillType& eSkill, int)
    {
        const BurnoutSkillzData::EBurnoutSkillType eOld = eSkill;
        eSkill = static_cast<BurnoutSkillzData::EBurnoutSkillType>(static_cast<int>(eSkill) + 1);
        CGS_ASSERT(static_cast<int>(eSkill) <= BurnoutSkillzData::E_BURNOUT_SKILL_COUNT,
                   "leEnumIndex <= BurnoutSkillzData::E_BURNOUT_SKILL_COUNT");
        return eOld;
    }

    // ------------------------------------------------------------------------
    // BurnoutSkillzData::Clear  (X360 0x8230FBF0)
    // ------------------------------------------------------------------------
    // Zero every skill accumulator. The asm walks one f32 at a time (stfs 0.0;
    // ptr += 4) for E_BURNOUT_SKILL_COUNT (== 14) elements, firing the iterating
    // enum's bounds assert each step. Expressed by name as the indexed clear over
    // mafBurnoutSkilz, driven by the post-increment iterator so the per-step assert
    // is preserved. (The X360 ABI returns the base float* in r3; the home declares
    // void Clear() -- the returned pointer is an ABI artifact with no caller use.)
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
    // BurnoutSkillzData::GetSkillAccuracy  (X360 0x8230FCD8)
    // ------------------------------------------------------------------------
    // Return the rounding accuracy used for one skill sample. Two leading range
    // guards (eSkill >= E_BURNOUT_SKILL_START; eSkill < E_BURNOUT_SKILL_COUNT), then
    // return the fixed accuracy constant the asm loads (lfs f1, flt_82020B48 ==
    // 0.0049999999). The "cell/ppu" phantom dep is this libm-style constant return;
    // no math call is actually performed, so a plain literal matches the asm.
    //
    // RETURN-TYPE FLAG: the X360 ABI / DecFIGS DWARF declare this as `double`
    // (0x8230FD44 leaves the value in f1; the pseudocode renders `return 0.0049999999`).
    // The home commits the return as `f32`. Bodied against the committed `f32` here
    // (the constant the asm loads is single-precision via lfs); FLAGGED for the
    // consolidator rather than retyping the committed home.
    f32 BurnoutSkillzData::GetSkillAccuracy(EBurnoutSkillType eSkill)
    {
        CGS_ASSERT(eSkill >= E_BURNOUT_SKILL_START, "leSkillType >= E_BURNOUT_SKILL_START");
        CGS_ASSERT(eSkill < E_BURNOUT_SKILL_COUNT, "leSkillType < E_BURNOUT_SKILL_COUNT");

        return 0.0049999999f;
    }

    // ------------------------------------------------------------------------
    // BurnoutSkillzData::SetBurnoutSkill  (X360 0x8231C9A8)
    // ------------------------------------------------------------------------
    // Store one skill sample, snapped to the skill's rounding accuracy. Two leading
    // range guards (eSkill in [E_BURNOUT_SKILL_START, E_BURNOUT_SKILL_COUNT)), then:
    //   * GetSkillAccuracy(eSkill) is called for the per-skill accuracy (0x8231CA14 bl;
    //     it also re-fires the range asserts). The result feeds the rounder's accuracy.
    //   * RoundFloatToAccuracy(&lfVal, accuracy) snaps the sample in place
    //     (0x8231CA1C bl; the asm passes the stack slot holding the incoming sample).
    //   * the rounded sample is stored at mafBurnoutSkilz[eSkill] (0x8231CA28 stfsx).
    //
    // ARG-MAPPING NOTE: the X360 export takes this=r3 and the skill index=r4 (eSkill),
    // with the incoming sample in f1 (stashed to a stack slot at 0x8231C9B8). Hex-Rays
    // mints a large garbage arg list (a3..a23 + a24) -- those are dropped/uninitialised
    // registers, not real parameters. The call site (the sole caller
    // BrnGameState::BurnoutSkillzManager::SetNewSkillIfGreater) sets up only r3/r4/f1,
    // confirming the home's (EBurnoutSkillType eSkill, f32 fVal) signature.
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
