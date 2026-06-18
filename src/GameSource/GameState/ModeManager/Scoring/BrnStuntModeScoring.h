#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h
// ============================================================================
// MINIMAL-SLICE home for BrnGameState::StuntModeScoring (+ the small POD value types it
// publishes: StuntInfo, StuntTypeInfo, StuntToDisplay).
//
// This is the per-mode sub-scorer the ScoringSystem keystone EMBEDS BY VALUE. The slice
// exists so ScoringSystem (and its dependents) can name the type, embed it, and call into
// its public surface and compile. The full layout + every method BODY land with this type's
// OWN TU (BrnStuntModeScoring.cpp) later.
//
// SHAPE is DWARF-authoritative
// (references/DecFIGS/dwarfdump/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h):
//   - StuntModeScoring is a plain struct, NO base class (DWARF shows no inheritance).
//   - The scalar/POD prefix of the member run (miCurrentScore .. mfPendingScoreTimer) is named
//     verbatim from the DWARF in declared order+types.
//   - The three template-instance members the DWARF lists last --
//        FixedRingBuffer<Vector3,256> mRecentJumpSet      (KI_MAX_RECENT_JUMPS = 256)
//        Set<CgsID,512>               mRecentStuntElementSet
//        FixedRingBuffer<u16,64>      mRecentPropSet       (KI_MAX_RECENT_PROPS = 64)
//        bool                         mbEndlessStuntRun
//        AchievementManager*          mpAchievementManager
//     -- depend on the FixedRingBuffer<> / Set<> container templates, which have no committed
//     home in the repo yet. To keep this header self-contained + compilable they are collapsed
//     into a single NOMINAL reserved-storage block (maReservedContainers). The real members land
//     when this type's TU (and those container templates) are reconstructed.
//
// Methods are DECLARE-ONLY (their bodies live in BrnStuntModeScoring.cpp). Signatures
// (return type / const / params) are taken from the DWARF. The action / output-interface
// parameter types are used only BY POINTER, so forward declarations suffice.
//
// EMBED-BY-VALUE rule: ScoringSystem names members + calls methods; it does NOT depend on a
// byte-exact sizeof. NOT byte-verified. Single owner -- grow this slice, do not fork.

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector3
#include "GameSource/GameState/BrnGameStateTypes.h"           // BrnGameState::EStuntType

namespace BrnWorld
{
    namespace RaceCarEntityModuleIO
    {
        // DWARF typedef: ActiveRaceCarOutputInterface == this type. Used by pointer only.
        struct RCEntityActiveRaceCarOutputInterface;
    }
}

namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        // Used by pointer only in DealWithStunt / DealWithPowerPark.
        struct WorldStuntAction;
        struct PowerParkResultAction;
    }

    // DWARF BrnStuntModeScoring.h:36 -- the achievement-manager member is a pointer to this
    // (typedef AchievementManager == BrnGameState::AchievementManagerPS3). Pointer only.
    class AchievementManagerPS3;

    // ------------------------------------------------------------------------
    // POD value types published by the stunt scorer.
    // ------------------------------------------------------------------------

    // DWARF BrnStuntModeScoring.h:98. Snapshot of a completed stunt handed back to callers via
    // WasStuntRecentlyPerformed().
    struct StuntInfo
    {
        u32 muStuntTypes;          // :100 -- bit-mask of EStuntType categories in the stunt
        u32 muAwesomeStuntTypes;   // :101 -- subset rated "awesome"
        s32 miStuntScore;          // :102
        s32 miStuntMultiplier;     // :103
        u16 muFlatSpins;           // :104
        u16 muBarrelRolls;         // :105
    };

    // DWARF BrnStuntModeScoring.h:74. Per-category running state used while rating a stunt.
    struct StuntTypeInfo
    {
        f32  mfTimeActive;     // :76
        f32  mfTimeSinceLast;  // :77
        f32  mfScore;          // :78
        bool mbActive;         // :79
    };

    // DWARF BrnStuntModeScoring.h:84. A single stunt entry surfaced to the HUD by
    // OutputStuntsToDisplay().
    struct StuntToDisplay
    {
        EStuntType meStuntType;  // :92
        s32        miStuntScore; // :93

        void Construct();        // :87  (declare-only -- body in StuntModeScoring's TU)
        bool IsValid() const;    // :90  (declare-only)
    };

    // ------------------------------------------------------------------------
    // The stunt-mode sub-scorer (embedded by value in ScoringSystem).
    // ------------------------------------------------------------------------
    // DWARF home BrnStuntModeScoring.h:117. No base class.
    struct StuntModeScoring
    {
    public:
        // --- public surface (DECLARE-ONLY; bodies live in BrnStuntModeScoring.cpp) ---

        // DWARF typedef BrnStuntModeScoring.h:67 -- the output interface Update/UpdateXxx read.
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
            ActiveRaceCarOutputInterface;
        // DWARF typedef BrnStuntModeScoring.h:36 -- the achievement-manager member type.
        typedef BrnGameState::AchievementManagerPS3 AchievementManager;

        void       Construct(AchievementManager* lpAchievementManager);                 // :123
        bool       Prepare();                                                           // :127
        void       Update(const ActiveRaceCarOutputInterface* lpRaceCar, f32 lfDelta);  // :133
        bool       Release();                                                           // :137
        void       Destruct();                                                          // :141
        void       Activate(s32 liTargetScore);                                         // :145
        void       ClearData();                                                         // :149
        void       OutputStuntsToDisplay(s32 liCount, StuntToDisplay* lpStunts);        // :153

        s32        GetCurrentScore() const;                                             // :157
        s32        GetTargetScore() const;                                              // :160
        s32        GetComboScore() const;                                               // :163
        s32        GetComboMultiplier() const;                                          // :166
        u32        GetCurrentStunts() const;                                            // :170
        u32        GetAllStuntTypesForInProgressStunt() const;                          // :174
        bool       HasTargetScoreBeenExceeded() const;                                  // :177
        bool       IsComboInProgress() const;                                           // :180
        bool       IsComboWarningActive() const;                                        // :183
        f32        GetTimeSinceComboWarningActivated() const;                           // :187

        // VIRTUAL -- dispatched through the vtable, NOT a direct call. The keystone
        // ScoringSystem::HasStuntAttackModeEnded (X360 0x82326708) calls this as
        //   v6 = *(scorer);  (*(v6 + 0x14))(scorer, HasModeTimeExpired())
        // i.e. through vtable slot +0x14 on the embedded scorer (ScoringSystem this+0x350
        // for the offline path, this+0x2620 for the online path). The DERIVED online
        // variant BrnGameState::StuntModeScoringOnline (X360 0x82313680) OVERRIDES this and
        // forwards to the base StuntModeScoring::HasStuntModeEnded (X360 0x82313518), so the
        // base must be virtual for the override + the dispatch to bind. DWARF spelt the
        // method non-virtual (it dropped the virtuality annotation), but the X360 asm proves
        // the polymorphism; we trust the asm. Signature `bool HasStuntModeEnded(bool)` is
        // DWARF-authoritative (BrnStuntModeScoring.h:191; .cpp:129 names the param lbTimeUp).
        //
        // SLOT NOTE: the StuntModeScoring vtable is larger than this one entry -- other
        // StuntModeScoring methods are dispatched at slots +0x1C, +0x20 and +0x28 in the asm.
        // Reconstructing that full vtable ORDER (so HasStuntModeEnded lands at exactly +0x14)
        // is deferred to this type's own TU; we do NOT fabricate phantom virtual slots here.
        // Declaring this single virtual is sufficient for the HasStuntAttackModeEnded /
        // StopModeTimer callers to compile (semantic parity; no vtable-layout assert).
        virtual bool HasStuntModeEnded(bool lbTimeUp);                                  // :191

        void       DealWithStunt(const GameStateModuleIO::WorldStuntAction* lpAction);  // :195
        void       DealWithHitProp(u16 luPropId, u8 luFlags);                           // :199
        void       DealWithPowerPark(const GameStateModuleIO::PowerParkResultAction* lpAction); // :203

        bool       WasStuntRecentlyPerformed(StuntInfo* lpStuntInfo);                   // :208
        bool       WasComboRecentlyPerformed(s32* lpScore, bool* lpValid);              // :214
        bool       WasTimeRecentlyUp();                                                 // :218

    protected:
        // --- protected helpers (DECLARE-ONLY) ---
        void       BeginCombo();                                                        // :224
        void       EndCombo();                                                          // :229
        void       ClearStuntTypeInfo();                                                // :233
        void       UpdateCombo(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar); // :239
        void       UpdateBufferedScore(f32 lfDelta);                                    // :242
        void       UpdateStuntRepetition(f32 lfDelta);                                  // :245
        bool       UpdateAirStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar);     // :250
        bool       UpdateDriftStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar);   // :255
        bool       UpdateBoostStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar);   // :260
        bool       UpdateDrivingStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar); // :265
        bool       UpdateCrashStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar);   // :270
        void       UpdateStuntRating(EStuntType leStuntType, f32 lfA, f32 lfB, f32 lfC);            // :277
        void       UpdateScore(f32 lfScore, EStuntType leStuntType, bool lbAwesome);    // :283
        bool       RegisterStunt();                                                     // :287
        f32        GetRepetitionScoreFalloff(EStuntType leStuntType) const;             // :291
        f32        GetMinimumScoreAward(EStuntType leStuntType) const;                  // :295
        f32        GetTakeOffMultiplier() const;                                        // :299
        bool       IsStuntTypeInProgress(EStuntType leStuntType) const;                 // :303
        bool       HasAnyPendingScore() const;                                          // :307
        bool       ShouldBankScore() const;                                             // :311

    private:
        // --- data members (DWARF declared order + types) ---
        // Named scalar/POD prefix of the member run.
        s32     miCurrentScore;                 // :315
        s32     miTargetScore;                  // :316
        f32     mfPendingNonGuaranteedScore;    // :317
        f32     mfPendingGuaranteedScore;       // :318
        f32     mfComboScore;                   // :319
        s32     miComboMultiplier;              // :320

        bool    mbStuntModeActive;              // :322
        bool    mbStuntInProgress;              // :323
        bool    mbComboInProgress;              // :324
        bool    mbValidStunt;                   // :325
        bool    mbWasInAirLastFrame;            // :326
        bool    mbInitialDriftOngoing;          // :327
        bool    mbTimeLimitExpired;             // :328
        bool    mbTimeUpMessageSent;            // :329
        bool    mbPlayerCarCrashing;            // :330

        Vector3 mStuntRollInProgress;           // :331
        u32     muStuntTypesInProgress;         // :332
        u32     muAwesomeStuntTypesInProgress;  // :333
        f32     mfTimeSinceLastStunt;           // :334
        f32     mfSpeedMPHBeforeCrashing;       // :335
        f32     mfTimeDelayBeforeModeEnd;       // :336

        bool          mbRecentStunt;            // :338
        StuntInfo     mRecentStunt;             // :339
        bool          mbRecentCombo;            // :340
        s32           miRecentComboScore;       // :341
        f32           mfPendingScoreTimer;      // :342

        // KI_NUM_STUNTS_TO_DISPLAY-adjacent: per-category rating state, E_STUNT_TYPE_COUNT (15) wide.
        StuntTypeInfo mStuntTypeInfo[15];       // :344

        // NOMINAL -- full layout deferred to this type's own TU.
        // Collapses the remaining DWARF members that depend on the not-yet-reconstructed
        // FixedRingBuffer<> / Set<> container templates and the trailing scalars/pointer:
        //   FixedRingBuffer<Vector3,256> mRecentJumpSet         (:346)
        //   Set<CgsID,512>               mRecentStuntElementSet (:348)
        //   FixedRingBuffer<u16,64>      mRecentPropSet         (:350)
        //   bool                         mbEndlessStuntRun      (:352)
        //   AchievementManager*          mpAchievementManager   (:354)
        // The size below is a generous nominal estimate (Vector3[256] dominates) -- NOT byte-exact,
        // and not relied upon (embed-by-value + named member access, no sizeof assert).
        u8 maReservedContainers[256 * 16 + 512 * 8 + 64 * 2 + 16]; // NOMINAL
    };
}
