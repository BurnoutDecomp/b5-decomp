// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring_Queries.cpp
// ============================================================================
// Reconstructed BODIES (store-for-store from the X360 dossiers) for the read-only /
// "what just happened" query surface of BrnGameState::StuntModeScoring. This is a
// PARTIAL TU for the type whose home is BrnStuntModeScoring.h; the rest of the
// StuntModeScoring methods (Update*, DealWith*, lifecycle, ...) land in sibling TUs.
//
// Methods bodied here (X360 ARTIST.XEX addresses):
//   GetCurrentStunts            0x82310640
//   ShouldBankScore             0x82313208
//   HasAnyPendingScore          0x82313198
//   HasStuntModeEnded           0x82313518   (virtual; base impl)
//   WasStuntRecentlyPerformed   0x82313280
//   WasComboRecentlyPerformed   0x823132D0
//   WasTimeRecentlyUp           0x82313330
//   OutputStuntsToDisplay       0x823211E8
//
// Each body is a clean (de-optimized) translation of its X360 pseudocode/asm.
// Members are accessed BY NAME against the committed layout in BrnStuntModeScoring.h;
// every `*(a1+offset)` in the asm was reconciled to a named member by offset+role
// (the per-method notes below record the offset->member mapping). Semantic parity,
// NOT byte-matching.
//
// FLAGS (things the recovered asm forced, recorded for the reviewer):
//  * mStuntTypeInfo is [18] in its committed home: the three array-scanning bodies
//    here iterate indices 0..17 (`< 18`). (Grown in round 1; matches the asm `cmpwi 0x12`.)
//  * WasComboRecentlyPerformed has a third out-param (f32* lpComboTimer) in its
//    committed home: the DWARF spelt 2 out-params, the X360 body writes 3.
//  * WasComboRecentlyPerformed's validity flag derives from miCurrentScore (+0x10),
//    NOT mfComboScore (the round-1 note was WRONG; fixed -- see method below). The
//    third out-param is filled from mfRecentComboTime (+0x88), NOT mfPendingScoreTimer.
//  * TidyStuntScore is declared+inline-bodied in BrnStuntModeScoring.h (its DWARF home
//    for this TU family); OutputStuntsToDisplay calls it directly -- no local decl needed.

#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"
#include "GameSource/GameState/BrnGameActions.h"        // GameStateModuleIO action types (TU family)
#include "GameSource/GameState/BrnGameStateTypes.h"     // EStuntType, E_STUNT_TYPE_INVALID
// (no CGS_ASSERT in any recovered query body -- none of these X360 functions assert.)

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // Reconstruction constants (named per CXX_NAMING_CONVENTIONS).
    // ------------------------------------------------------------------------
    namespace
    {
        // The full per-category rating array spans every EStuntType index 0..17
        // (categories 0-14 + the error/rating pseudo-types 15-17). The X360 loops use
        // the literal bound 18 (`cmpwi ...,0x12; blt`).
        const s32 KI_STUNT_TYPE_INFO_COUNT = 18;

        // Bit-mask (over EStuntType bit positions) of the stunt categories that are
        // "bankable" -- i.e. whose in-progress activity must finish before a score is
        // committed. Verbatim from the X360 ShouldBankScore body (lis 1; ori ...,0x1EF7).
        const u32 KU_BANKABLE_STUNT_TYPE_MASK = 0x11EF7u;

        // The decompiler's symmetric "is ~zero" tolerance (2^-23). The X360 pending-
        // score / display-score sign tests treat |x| <= this as zero (flt_82020B30 is
        // +eps, flt_82002514 is -eps).
        const f32 KF_SCORE_EPSILON = 1.1920929e-7f;
    }

    // ------------------------------------------------------------------------
    // GetCurrentStunts  (X360 0x82310640)
    // Build a bit-mask of the stunt categories that are currently active. The X360
    // body walks mStuntTypeInfo[0..17] (base a1+0xAC == &mStuntTypeInfo[1].mbActive, the
    // unroll reads mbActive at element-stride 16) and, for each whose mbActive is set,
    // ORs in bit <index>. (The asm's 6-wide unroll + the v2 base-2 bit bookkeeping is a
    // compiler artifact of a single 0..17 loop; reconstructed as the clean loop.)
    // ------------------------------------------------------------------------
    u32 StuntModeScoring::GetCurrentStunts() const
    {
        u32 luStuntsMask = 0;
        for (s32 liType = 0; liType < KI_STUNT_TYPE_INFO_COUNT; ++liType)
        {
            if (mStuntTypeInfo[liType].mbActive)
            {
                luStuntsMask |= (1u << liType);
            }
        }
        return luStuntsMask;
    }

    // ------------------------------------------------------------------------
    // ShouldBankScore  (X360 0x82313208)
    // A pending score may be banked only once no "bankable" category is still being
    // rated. If any mStuntTypeInfo[i].mbActive is set (base a1+0x9C == &mStuntTypeInfo[0]
    // .mbActive, stride 16) for a category in the bankable mask, hold off (return false).
    // Otherwise bank if either a bankable stunt-type is flagged in-progress
    // (muStuntTypesInProgress @+0x50) or no stunt is in progress at all (mbStuntInProgress
    // @+0x29 == 0).
    // ------------------------------------------------------------------------
    bool StuntModeScoring::ShouldBankScore() const
    {
        for (s32 liType = 0; liType < KI_STUNT_TYPE_INFO_COUNT; ++liType)
        {
            if (((1u << liType) & KU_BANKABLE_STUNT_TYPE_MASK) != 0 &&
                mStuntTypeInfo[liType].mbActive)
            {
                return false;
            }
        }
        return (muStuntTypesInProgress & KU_BANKABLE_STUNT_TYPE_MASK) != 0 ||
               !mbStuntInProgress;
    }

    // ------------------------------------------------------------------------
    // HasAnyPendingScore  (X360 0x82313198)
    // True while either pending-score accumulator is meaningfully non-zero. The X360
    // body tests the GUARANTEED accumulator first (mfPendingGuaranteedScore @+0x1C), then
    // the NON-guaranteed (mfPendingNonGuaranteedScore @+0x18), returning true as soon as
    // one is outside the +/- epsilon dead-band.
    // ------------------------------------------------------------------------
    bool StuntModeScoring::HasAnyPendingScore() const
    {
        const f32 lfGuaranteed = mfPendingGuaranteedScore;   // +0x1C (tested first)
        const bool lbGuaranteedIsZero =
            (lfGuaranteed <= KF_SCORE_EPSILON) && (lfGuaranteed >= -KF_SCORE_EPSILON);
        if (!lbGuaranteedIsZero)
        {
            return true;
        }

        const f32 lfNonGuaranteed = mfPendingNonGuaranteedScore;  // +0x18 (tested second)
        const bool lbNonGuaranteedIsZero =
            (lfNonGuaranteed <= KF_SCORE_EPSILON) && (lfNonGuaranteed >= -KF_SCORE_EPSILON);
        if (!lbNonGuaranteedIsZero)
        {
            return true;
        }

        return false;
    }

    // ------------------------------------------------------------------------
    // HasStuntModeEnded  (X360 0x82313518) -- VIRTUAL base impl.
    // Latches the "time is up" flag (mbTimeLimitExpired @+0x2E = lbTimeUp) and reports
    // whether the stunt mode may now end. It may end only when: no stunt is in progress
    // (mbStuntInProgress @+0x29), no combo is in progress (mbComboInProgress @+0x2A),
    // time IS up, the player car is not crashing (mbPlayerCarCrashing @+0x30), the
    // endless-run flag is clear (mbEndlessStuntRun @+0x22BC), and the mode-end delay timer
    // has elapsed (mfTimeDelayBeforeModeEnd @+0x60 <= 0).
    //
    // NOTE: the asm reads mbStuntInProgress BEFORE storing lbTimeUp into mbTimeLimitExpired
    // (the store is unconditional and happens regardless of the early-outs below).
    // ------------------------------------------------------------------------
    bool StuntModeScoring::HasStuntModeEnded(bool lbTimeUp)
    {
        const bool lbStuntInProgress = mbStuntInProgress;   // +0x29 (read before the store)
        mbTimeLimitExpired = lbTimeUp;                      // +0x2E (unconditional latch)

        if (lbStuntInProgress)
        {
            return false;
        }
        if (mbComboInProgress)          // +0x2A
        {
            return false;
        }
        if (!lbTimeUp)
        {
            return false;
        }
        if (mbPlayerCarCrashing)        // +0x30
        {
            return false;
        }
        if (mbEndlessStuntRun)          // +0x22BC
        {
            return false;
        }
        if (mfTimeDelayBeforeModeEnd > 0.0f)   // +0x60
        {
            return false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // WasStuntRecentlyPerformed  (X360 0x82313280)
    // One-shot consumer of the "a stunt just completed" latch: if mbRecentStunt (@+0x64)
    // is set, copy the cached StuntInfo out to the caller (the asm copies 6 dwords ==
    // mRecentStunt @+0x68, the full StuntInfo POD), clear the latch, and report true.
    // ------------------------------------------------------------------------
    bool StuntModeScoring::WasStuntRecentlyPerformed(StuntInfo* lpStuntInfo)
    {
        if (!mbRecentStunt)             // +0x64
        {
            return false;
        }

        *lpStuntInfo = mRecentStunt;    // +0x68, 6 dwords == StuntInfo
        mbRecentStunt = false;
        return true;
    }

    // ------------------------------------------------------------------------
    // WasComboRecentlyPerformed  (X360 0x823132D0)
    // One-shot consumer of the "a combo just completed" latch. If mbRecentCombo (@+0x80)
    // is set, hand back the combo score (miRecentComboScore @+0x84), a validity flag, and
    // the recent-combo timer (mfRecentComboTime @+0x88); clear the latch; report true.
    //
    // FIX (round 2): the validity flag is `miRecentComboScore >= miCurrentScore / 2`,
    // where the divisor is miCurrentScore (+0x10) -- NOT mfComboScore. The asm proves it:
    //   lwz r10, 0x10(r11)   ; r10 = miCurrentScore          (+0x10, NOT mfComboScore @+0x20)
    //   srawi r10, r10, 1    ; \ signed divide-by-2,
    //   addze r10, r10       ; / rounding toward zero
    //   cmpw  r9, r10        ; r9 = miRecentComboScore (@+0x84) compared >= the halved score
    // `miCurrentScore / 2` in C++ is signed truncation toward zero, exactly matching
    // srawi+addze. The third out-param is the recent-combo timer mfRecentComboTime (+0x88,
    // `lfs f0, 0x88(r11)`), NOT mfPendingScoreTimer (+0x8C).
    // ------------------------------------------------------------------------
    bool StuntModeScoring::WasComboRecentlyPerformed(s32* lpScore, bool* lpValid, f32* lpComboTimer)
    {
        if (!mbRecentCombo)             // +0x80
        {
            return false;
        }

        *lpScore = miRecentComboScore;                          // +0x84
        *lpValid = miRecentComboScore >= (miCurrentScore / 2);  // +0x84 vs (+0x10)/2
        *lpComboTimer = mfRecentComboTime;                      // +0x88
        mbRecentCombo = false;
        return true;
    }

    // ------------------------------------------------------------------------
    // WasTimeRecentlyUp  (X360 0x82313330)
    // One-shot "the clock just ran out" edge. Fires true exactly once: when time has
    // expired (mbTimeLimitExpired @+0x2E), the message has not already been sent
    // (!mbTimeUpMessageSent @+0x2F), and a combo is in progress (mbComboInProgress @+0x2A).
    // Latches mbTimeUpMessageSent so it cannot fire again.
    // ------------------------------------------------------------------------
    bool StuntModeScoring::WasTimeRecentlyUp()
    {
        if (!mbTimeLimitExpired || mbTimeUpMessageSent || !mbComboInProgress)
        {
            return false;
        }

        mbTimeUpMessageSent = true;     // +0x2F
        return true;
    }

    // ------------------------------------------------------------------------
    // OutputStuntsToDisplay  (X360 0x823211E8)
    // Fill the caller's StuntToDisplay[liCount] array with the top `liCount` active stunt
    // categories, highest score first. Each pass scans mStuntTypeInfo[0..17] (base
    // a1+0x98 == &mStuntTypeInfo[0].mfScore; per-element reads .mfScore@+0 and .mbActive@+4
    // of the pointer, i.e. the score/active fields of each StuntTypeInfo) for the active
    // category whose (tidied) score is the largest still strictly below the previous pass's
    // ceiling -- a descending selection sort surfacing at most liCount entries. Unfilled /
    // no-match slots are left with meStuntType = E_STUNT_TYPE_INVALID (-1); the scan stops
    // early once a pass produces no score outside the +/- epsilon dead-band.
    // ------------------------------------------------------------------------
    void StuntModeScoring::OutputStuntsToDisplay(s32 liCount, StuntToDisplay* lpStunts)
    {
        const f32 lfFloatMax = 3.4028235e38f;   // flt_82020AFC: initial ceiling (FLT_MAX)

        f32 lfBestThisPass = 0.0f;              // v6: running best (init 0.0 == flt_82001CC0)
        f32 lfCeiling = lfFloatMax;             // v7: each pass must beat this strictly downward

        for (s32 liOut = 0; liOut < liCount; ++liOut)
        {
            // No qualifying score found yet this pass -> default the slot to "invalid".
            lpStunts[liOut].meStuntType = E_STUNT_TYPE_INVALID;   // *v3 = -1

            for (s32 liType = 0; liType < KI_STUNT_TYPE_INFO_COUNT; ++liType)
            {
                if (mStuntTypeInfo[liType].mbActive)              // *(v12+4)
                {
                    const f32 lfScore = mStuntTypeInfo[liType].mfScore;   // *v12
                    if (lfScore > lfBestThisPass && lfScore < lfCeiling)
                    {
                        lfBestThisPass = TidyStuntScore(lfScore);
                        lpStunts[liOut].meStuntType = (EStuntType)liType;
                        lpStunts[liOut].miStuntScore = (s32)lfBestThisPass;  // fctiwz
                    }
                }
            }

            // Stop once this pass produced no score outside the +/- epsilon dead-band.
            if (lfBestThisPass <= KF_SCORE_EPSILON && lfBestThisPass >= -KF_SCORE_EPSILON)
            {
                break;
            }

            lfCeiling = lfBestThisPass;   // next pass must be strictly smaller (v7 = v6)
            lfBestThisPass = 0.0f;        // reset the running best (v6 = v5 == 0.0)
        }
    }
}
