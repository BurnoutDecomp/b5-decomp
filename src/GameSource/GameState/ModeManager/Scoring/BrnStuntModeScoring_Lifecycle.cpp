#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"

#include "BrnCommonTypes.h"                            // Vector3 (mStuntRollInProgress.SetZero)
#include "GameSource/GameState/BrnGameActions.h"       // GameStateModuleIO action types (WorldStuntAction etc.)
#include "GameSource/GameState/BrnGameStateTypes.h"    // BrnGameState::EStuntType
#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT

// ============================================================================
// BrnStuntModeScoring_Lifecycle.cpp
// ============================================================================
// Bodies for the "Lifecycle" group of BrnGameState::StuntModeScoring (home header
// BrnStuntModeScoring.h): Construct, Activate, ClearData and the ClearStuntTypeInfo
// helper they share. Each body is reconstructed store-for-store from its X360
// pseudocode (addresses in the per-method comments) and the DecFIGS dwarfdump variable
// hints, accessing StuntModeScoring members BY NAME and the embedded containers through
// their modelled FixedRingBuffer / Set interface (CgsRingBuffer.h / CgsSet.h).
//
// SCOPE NOTE -------------------------------------------------------------------------
// The home header has since been GROWN with the three members the X360 ClearData body
// touches that the DecFIGS DWARF had dropped, so every ClearData store now maps to a
// committed, named member and IS emitted:
//   * X360 +0x1B0  -- maStuntTypeScoreCount[18] (ClearData's second loop: mtctr 18,
//                     stw 0, stride 4, base this+0x1B0, immediately after mStuntTypeInfo[18]).
//   * X360 +0x22BD -- mbStuntInfoEventPending (ClearData also clears it; PreWorldUpdate
//                     0x823446F8 gates on it / queues the event / clears it).
// NOTE on +0x88 (mfRecentComboTime): the round-1 draft FABRICATED a `*(this+0x88)` store
// into ClearData. The recovered X360 body (0x82321108) does NOT store +0x88 -- it stores
// +0x8C (mfPendingScoreTimer) but never +0x88. ClearData does NOT touch mfRecentComboTime;
// that member is only written by EndCombo (0x823215D8) and read by WasComboRecentlyPerformed
// (0x823132D0). The fabricated claim has been removed.
//
// mfPendingScoreTimer is reset to KF_PENDING_SCORE_TIMER_NOT_STARTED (X360 flt_82CDB784,
// BrnStuntModeScoring.cpp:57). That file-scope constant is DECLARE-ONLY here (extern); its
// single definition now lives in the sibling BrnStuntModeScoring_UpdatePass.cpp, whose
// UpdateBufferedScore is the constant's other consumer. Until that definition landed the extern
// resolved to nothing anywhere in the tree (a latent LNK2001 that the cl /c gate cannot catch),
// while _UpdatePass.cpp carried an unrelated anonymous-namespace copy of the same name -- two
// spellings of one constant with two different values. When the full BrnStuntModeScoring.cpp is
// reconstructed it takes ownership of the definition and this extern re-points at it.
//
// Construct's X360 body issues a vtable call (slot +0x10) on `this` which is the virtual
// dispatch to ClearData (the derived StuntModeScoringOnline overrides ClearData). The full
// StuntModeScoring vtable order is deferred to this type's own TU (see the header note); for
// semantic parity the base Construct simply calls ClearData() directly here.
// ============================================================================

namespace BrnGameState
{
    // DECLARE-ONLY: file-scope tunable DEFINED in BrnStuntModeScoring_UpdatePass.cpp as
    // 1000.0f (X360 flt_82CDB784, VA 0x82CDB784, read big-endian from the decrypted image).
    // The "pending-score timer not running" sentinel ClearData parks mfPendingScoreTimer at --
    // a sentinel, not a duration: the consumer guard is `mfPendingScoreTimer >= NOT_STARTED`
    // (asm 0x8232C19C), so it has to sit above every real timer value. `const` here matches the
    // definition's linkage; a non-const extern would not bind to it.
    extern const f32 KF_PENDING_SCORE_TIMER_NOT_STARTED;

    namespace
    {
        // RwMathFPU::IsZero's +/- dead-band (X360 epsilon 1.1920929e-7). Matches the inline
        // idiom the sibling BrnStuntModeScoring_Queries.cpp uses for the same RwMathFPU::IsZero
        // checks (RwMathFPU has no committed home; the comparison is reproduced inline).
        const f32 KF_SCORE_EPSILON = 1.1920929e-7f;

        inline bool RwMathFPU_IsZero(f32 lfValue)
        {
            return (lfValue <= KF_SCORE_EPSILON) && (lfValue >= -KF_SCORE_EPSILON);
        }
    }

// ----------------------------------------------------------------------------
// Construct  --  X360 0x8232C080
// ----------------------------------------------------------------------------
// One-time construction: attach the three embedded containers' inline storage (each
// Construct() resets positions/length to empty), mark this as a finite (non-endless) run,
// run a full ClearData() to bring all scoring state to its zeroed start, then latch the
// achievement manager. Order matches the X360 asm (mbEndlessStuntRun before ClearData,
// mpAchievementManager after).
//
// X360 store map (a1 is int*, so a1[N] == this + 4*N):
//   a1[129..132]=256/0/0/0 + a1[128]=&a1[136]  -> mRecentJumpSet.Construct (capacity 256, attach)
//   a1[2188..2190]=0 + a1[2186]=&a1[2191] + a1[2187]=64 -> mRecentPropSet.Construct (capacity 64, attach)
//   a1[2184]=0                                  -> mRecentStuntElementSet.Construct (Set length 0)
//   *(a1+8892) = 0  (byte 0x22BC)               -> mbEndlessStuntRun = false
//   result = (*(*a1 + 16))()                    -> virtual dispatch slot +0x10 == ClearData()
//   a1[2224] = a2   (byte 0x22C0)               -> mpAchievementManager = lpAchievementManager
void StuntModeScoring::Construct(AchievementManager* lpAchievementManager)
{
    mRecentJumpSet.Construct();          // FixedRingBuffer<Vector3,256>::Construct (attach + Clear)
    mRecentPropSet.Construct();          // FixedRingBuffer<u16,64>::Construct (attach + Clear)
    mRecentStuntElementSet.Construct();  // Set<CgsID,512>::Construct (Clear -> empty-but-usable)

    mbEndlessStuntRun = false;

    ClearData();                         // X360: virtual dispatch through vtable slot +0x10

    mpAchievementManager = lpAchievementManager;
}

// ----------------------------------------------------------------------------
// Activate  --  X360 0x82312FA0
// ----------------------------------------------------------------------------
// Arm the stunt scorer for a fresh attack run against liTargetScore. The X360 body first
// asserts the scorer is in its cleared resting state (no banked score, no live combo, time
// not expired, car not crashing), then latches the target score and raises the two "now
// active" flags.
//   *(v2+4) -> miCurrentScore (0x10)        ; v2[8] -> mfComboScore (0x20)
//   *(v2+9) -> miComboMultiplier (0x24)     ; *(v2+46) -> mbTimeLimitExpired (0x2E)
//   *(v2+48) -> mbPlayerCarCrashing (0x30)
//   *(v2+5) = a2  -> miTargetScore (0x14)   ; *(v2+40) = 1 -> mbStuntModeActive (0x28)
//   *(v2+45) = 1  -> mbInitialDriftOngoing (0x2D)
void StuntModeScoring::Activate(s32 liTargetScore)
{
    CGS_ASSERT(miCurrentScore == 0, "miCurrentScore == 0");
    CGS_ASSERT(RwMathFPU_IsZero(mfComboScore), "RwMathFPU::IsZero(mfComboScore)");
    CGS_ASSERT(miComboMultiplier == 1, "miComboMultiplier == 1");
    CGS_ASSERT(!mbTimeLimitExpired, "!mbTimeLimitExpired");
    CGS_ASSERT(!mbPlayerCarCrashing, "!mbPlayerCarCrashing");

    miTargetScore         = liTargetScore;   // X360 stw a2, 0x14
    mbStuntModeActive     = true;            // X360 stb 1, 0x28
    mbInitialDriftOngoing = true;            // X360 stb 1, 0x2D
}

// ----------------------------------------------------------------------------
// ClearStuntTypeInfo  --  (inlined into ClearData / UpdateScores in the X360 build;
//                          DWARF BrnStuntModeScoring.cpp:1309, local liIndex at :1311)
// ----------------------------------------------------------------------------
// Reset every per-category rating record to its empty state: no time accumulated, no time
// since last, no score, inactive. Spans the full mStuntTypeInfo extent (X360 loop count 18:
// the 15 real EStuntType categories plus the 3 rating pseudo-slots). The store order inside
// each 16-byte record matches the X360 ClearData inline (mfScore @+8, mfTimeActive @+0,
// mbActive @+12, mfTimeSinceLast @+4).
void StuntModeScoring::ClearStuntTypeInfo()
{
    for (s32 liIndex = 0; liIndex < 18; ++liIndex)
    {
        mStuntTypeInfo[liIndex].mfTimeActive    = 0.0f;
        mStuntTypeInfo[liIndex].mfTimeSinceLast = 0.0f;
        mStuntTypeInfo[liIndex].mfScore         = 0.0f;
        mStuntTypeInfo[liIndex].mbActive        = false;
    }
}

// ----------------------------------------------------------------------------
// ClearData  --  X360 0x82321108
// ----------------------------------------------------------------------------
// Return the whole scorer to its zeroed start-of-run state: scores/timers to zero, the
// combo multiplier to its neutral 1, the pending-score timer parked at "not started", every
// boolean cleared (mbValidStunt is the one that resets to true), the in-progress stunt-roll
// vector and bit-masks zeroed, the per-category rating records cleared, the per-category hit
// counters zeroed, and the three recent containers emptied. Reconstructed store-for-store
// from the X360 asm. NB: the asm stores +0x8C (mfPendingScoreTimer) but never +0x88, so
// mfRecentComboTime is intentionally NOT touched here (only EndCombo writes it).
void StuntModeScoring::ClearData()
{
    miCurrentScore               = 0;       // X360 *(result+16)   stw 0, 0x10
    miTargetScore                = 0;       // X360 *(result+20)   stw 0, 0x14
    mfPendingNonGuaranteedScore  = 0.0f;    // X360 *(result+24)   stfs 0, 0x18
    mfPendingGuaranteedScore     = 0.0f;    // X360 *(result+28)   stfs 0, 0x1C
    mfComboScore                 = 0.0f;    // X360 *(result+32)   stfs 0, 0x20
    miComboMultiplier            = 1;       // X360 *(result+36)   stw 1, 0x24

    mbStuntModeActive            = false;   // X360 *(result+40)   stb 0, 0x28
    mbStuntInProgress            = false;   // X360 *(result+41)   stb 0, 0x29
    mbComboInProgress            = false;   // X360 *(result+42)   stb 0, 0x2A
    mbValidStunt                 = true;    // X360 *(result+43)   stb 1, 0x2B
    mbWasInAirLastFrame          = false;   // X360 *(result+44)   stb 0, 0x2C
    mbInitialDriftOngoing        = false;   // X360 *(result+45)   stb 0, 0x2D
    mbTimeLimitExpired           = false;   // X360 *(result+46)   stb 0, 0x2E
    mbTimeUpMessageSent          = false;   // X360 *(result+47)   stb 0, 0x2F
    mbPlayerCarCrashing          = false;   // X360 *(result+48)   stb 0, 0x30

    mStuntRollInProgress.SetZero();         // X360 stvx128 v0(=0), r3+0x40 (16-byte zero @0x40)
    muStuntTypesInProgress        = 0;      // X360 *(result+80)   stw 0, 0x50
    muAwesomeStuntTypesInProgress = 0;      // X360 *(result+84)   stw 0, 0x54

    mfTimeDelayBeforeModeEnd      = 0.0f;   // X360 *(result+96)   stfs 0, 0x60 (+0x60, NOT +0x58; +0x58 is not cleared by ClearData)

    mbRecentStunt                 = false;  // X360 *(result+100)  stb 0, 0x64
    mbRecentCombo                 = false;  // X360 *(result+128)  stb 0, 0x80
    mfPendingScoreTimer           = KF_PENDING_SCORE_TIMER_NOT_STARTED;  // X360 *(result+140) stfs flt_82CDB784, 0x8C

    mbStuntInfoEventPending       = false;  // X360 *(result+8893) stb 0, 0x22BD

    ClearStuntTypeInfo();                   // X360 first per-entry loop: v4=result+144(0x90), 18 iters, stride 16

    // X360 second loop: v5=result+432(0x1B0), v6=18, do { *v5++ = 0; } while(--v6) -- zero the
    // per-stunt-type hit-counter array (18 words at this+0x1B0, immediately after mStuntTypeInfo[18]).
    for (s32 liIndex = 0; liIndex < 18; ++liIndex)
    {
        maStuntTypeScoreCount[liIndex] = 0;
    }

    mRecentJumpSet.Clear();                 // X360 *(result+520/524/528) = 0  (ring base +0x200 Clear)
    mRecentPropSet.Clear();                 // X360 *(result+8752/8756/8760) = 0 (ring base +0x2228 Clear)
    mRecentStuntElementSet.Clear();         // X360 *(result+8736) = 0  (Set muLength = 0 @0x2220, Clear)
}

} // namespace BrnGameState
