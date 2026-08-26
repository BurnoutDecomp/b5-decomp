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
// ACCESSOR CLOSURE (2026-08-26). Five more members of the same read-only surface, all of
// which were declared-only and were REAL LINK RESIDUE of the scoring mount (they appear in
// scratch/stuntrace_scout/datafeed/objs/undef_demangled.txt). None of the five has its own
// out-of-line X360 symbol; each is recovered from a site that DOES:
//   Prepare                             vtable slot 1 of StuntModeScoring's vtable
//                                       (@0x820CEF50) == 0x82C296C8, the ICF-folded body
//                                       `li r3, 1 ; blr` -- i.e. `return true;`
//   GetComboScore                       X360 0x8232B180 (inlined in WriteDataToOutput)
//   GetAllStuntTypesForInProgressStunt  X360 0x8232B1A0 (same caller)
//   IsComboWarningActive                X360 0x8232B1A8..0x8232B1F0 (same caller)
//   GetTimeSinceComboWarningActivated   X360 0x8232B21C..0x8232B258 (same caller)
// The recovery site is ScoringSystem::WriteDataToOutput (X360 0x8232AE98), whose DecFIGS
// dwarfdump (BrnScoringSystem.cpp:799) states the SOURCE reaches the embedded scorer through
// these getters; the free build inlined them to raw offsets, so the offsets below are the
// getters' own bodies read back out of that caller.
//
// IsComboInProgress is NOT bodied here: X360 0x82313510 is `lbz r3, 0x2A(r3) ; blr` and the
// committed home already carries exactly that as a header inline (BrnStuntModeScoring.h:219),
// so an out-of-line definition here would be a second definition of the same function.
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

        // ---- combo-warning tuning pair (the X360 .data float block at 0x82CDB7xx) --------
        // Both are direct big-endian f32 reads of the decrypted ARTIST basefile at the two
        // VAs the asm names (image VA - 0x82000000 == file offset), the same recovery route
        // this TU family already used for flt_82CDB7D0 (== FLT_MAX, cross-checked against the
        // PS3 export in BrnScoringSystem_Lookup.cpp).
        //
        // The warning fires once the combo has gone this long without a scoring stunt. The
        // threshold is deliberately tiny -- one frame of no stunt arms the HUD warning.
        const f32 KF_COMBO_WARNING_TIME_THRESHOLD = 0.01f;   // flt_82CDB78C (0x3C23D70A)
        // ...and the warning's own display span: GetTimeSinceComboWarningActivated saturates
        // at this value. CROSS-CHECKED: it is the SAME constant UpdateCombo (X360 0x82320FF0)
        // compares mfTimeSinceLastStunt against to END the combo (`if (v5 > flt_82CDB790)`),
        // so the HUD warning bar runs 0 -> 5 s and the combo drops exactly when it fills.
        const f32 KF_COMBO_WARNING_DISPLAY_SPAN   = 5.0f;    // flt_82CDB790 (0x40A00000)
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

    // ------------------------------------------------------------------------
    // Prepare  (X360: StuntModeScoring vtable slot 1 @0x820CEF54 -> 0x82C296C8)
    // Per-event prep for the OFFLINE stunt scorer: nothing to do, report success.
    //
    // DERIVATION (no own symbol -- this is a virtual whose base implementation the linker
    // ICF-folded): ScoringSystem::Prepare (X360 0x8232A430) reaches it as
    //     lwz r11, 0x350(this) ; addi r3, this, 0x350 ; lwz r11, 4(r11) ; bctrl
    // i.e. a vtable dispatch at slot +4 on the embedded scorer (ScoringSystem+0x350 ==
    // mStuntModeScoring). Reading StuntModeScoring's vtable out of the image at 0x820CEF50
    // gives slot 1 == 0x82C296C8, whose whole body is `li r3, 1 ; blr`. That address is
    // COMDAT-FOLDED with several other trivial `return 1` leaves in the image (IDA names it
    // after one of them, CgsSound::Playback::Content::DoOnPostLoad) -- the fold is the proof
    // that StuntModeScoring::Prepare's own body is exactly `return true;` and nothing else.
    //
    // The whole vtable is pinned by three independently known slots, so the slot-1 read is a
    // transcription rather than a guess: slot 5 == 0x82313518 HasStuntModeEnded (the committed
    // header's documented vtable+0x14), slot 10 == 0x82312DE8 CalculateMultiplier (documented
    // vtable+0x28), and slot 4 == 0x82321108 ClearData (the target StuntModeScoring::Construct
    // 0x8232C080 dispatches through vtable+0x10).
    //
    // NOTE: the DERIVED online scorer overrides this with real work
    // (StuntModeScoringOnline::Prepare 0x82338B50, vtable slot 1 of ITS vtable @0x820CF9EC);
    // that override does NOT chain to this base, which is consistent with the base being inert.
    // ------------------------------------------------------------------------
    bool StuntModeScoring::Prepare()
    {
        return true;
    }

    // ------------------------------------------------------------------------
    // GetComboScore  (X360 0x8232B180, inlined in ScoringSystem::WriteDataToOutput)
    // The live combo score as a whole number. The X360 emits
    //     lfs    f0, 0x20(scorer)   ; mfComboScore
    //     fctiwz f0, f0             ; float -> int, ROUND TOWARD ZERO
    //     stfiwx f0, 0, r11
    // fctiwz is the truncating convert, which is exactly C++'s float->int conversion.
    // ------------------------------------------------------------------------
    s32 StuntModeScoring::GetComboScore() const
    {
        return static_cast<s32>(mfComboScore);   // +0x20, fctiwz == truncation
    }

    // ------------------------------------------------------------------------
    // GetAllStuntTypesForInProgressStunt  (X360 0x8232B1A0, same caller)
    // The bit-mask of every EStuntType the CURRENT (still-running) stunt has accumulated --
    // distinct from GetCurrentStunts(), which rebuilds a mask from the per-category
    // mStuntTypeInfo[].mbActive flags. The X360 is a single `lwz r11, 0x50(scorer)`, and
    // +0x50 is the committed muStuntTypesInProgress (the same member ShouldBankScore above
    // masks with KU_BANKABLE_STUNT_TYPE_MASK).
    // ------------------------------------------------------------------------
    u32 StuntModeScoring::GetAllStuntTypesForInProgressStunt() const
    {
        return muStuntTypesInProgress;   // +0x50
    }

    // ------------------------------------------------------------------------
    // IsComboWarningActive  (X360 0x8232B1A8..0x8232B1F0, inlined in WriteDataToOutput)
    // "The combo is about to be lost" HUD gate. Three conditions, short-circuited in this
    // order by the asm:
    //     lbz    r11, 0x2A(scorer)              ; mbComboInProgress -- else false
    //     lfs    f13, 0x20(scorer) ; fctiwz     ; (s32)mfComboScore  -- must be > 0
    //     lfs    f13, 0x58(scorer)              ; mfTimeSinceLastStunt
    //     fcmpu  cr6, f13, f0(flt_82CDB78C) ; bge -> true
    // (+0x58 is the committed mfTimeSinceLastStunt, whose asm-proven runtime role as the
    // "seconds since the last scoring stunt" combo timer is recorded in the home header.)
    // The score test goes through the INTEGER form -- the asm converts before comparing, so a
    // combo score in (0, 1) reads as 0 and suppresses the warning.
    // ------------------------------------------------------------------------
    bool StuntModeScoring::IsComboWarningActive() const
    {
        if (!mbComboInProgress)                                 // +0x2A
        {
            return false;
        }
        if (GetComboScore() <= 0)                               // (s32)+0x20
        {
            return false;
        }
        return mfTimeSinceLastStunt >= KF_COMBO_WARNING_TIME_THRESHOLD;   // +0x58
    }

    // ------------------------------------------------------------------------
    // GetTimeSinceComboWarningActivated  (X360 0x8232B21C..0x8232B258, same caller)
    // How far the combo-loss warning has progressed, in seconds, saturating at the warning's
    // full span. Zero while the warning is not active (the asm's `fmr f0, f31` arm, where f31
    // was loaded from flt_82001CC0 == 0.0f).
    //
    // The active arm is:
    //     lfs   f13, 0x58(scorer)                  ; mfTimeSinceLastStunt
    //     fsubs f0,  f13, f0(flt_82CDB78C)         ; elapsed past the warning threshold
    //     lfs   f13, flt_82CDB790                  ; the display span
    //     fsubs f12, f0, f13                       ; elapsed - span
    //     fsel  f0,  f12, f13, f0                  ; (elapsed - span) >= 0 ? span : elapsed
    // i.e. min(elapsed, span) -- fsel picks the span once the elapsed time has run past it.
    // The gate re-evaluates the SAME predicate IsComboWarningActive computes (the X360 emits
    // it twice, once per consumer); written here as the single named call.
    // ------------------------------------------------------------------------
    f32 StuntModeScoring::GetTimeSinceComboWarningActivated() const
    {
        if (!IsComboWarningActive())
        {
            return 0.0f;                                        // flt_82001CC0
        }

        const f32 lfElapsed = mfTimeSinceLastStunt - KF_COMBO_WARNING_TIME_THRESHOLD;
        return (lfElapsed >= KF_COMBO_WARNING_DISPLAY_SPAN) ? KF_COMBO_WARNING_DISPLAY_SPAN
                                                            : lfElapsed;   // fsel == min()
    }
}
