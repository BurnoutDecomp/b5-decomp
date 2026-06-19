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
//   - The three template-instance members the DWARF lists last are now the REAL members,
//     spelled with the just-modelled container templates:
//        CgsContainers::FixedRingBuffer<Vector3,256> mRecentJumpSet  (KI_MAX_RECENT_JUMPS = 256)
//        Set<CgsID,512>                              mRecentStuntElementSet
//        CgsContainers::FixedRingBuffer<u16,64>      mRecentPropSet   (KI_MAX_RECENT_PROPS = 64)
//        bool                                        mbEndlessStuntRun
//        AchievementManager*                         mpAchievementManager
//     FixedRingBuffer<> lives in namespace CgsContainers (CgsRingBuffer.h); Set<> at global
//     scope (CgsSet.h) -- both included above. The DWARF spells the jump-buffer element as
//     rw::math::vpu::Vector3; we use the project's Vector3 (BrnCommonTypes.h) per convention,
//     and CgsID (typedef u64) for the stunt-element set.
//
// Methods are DECLARE-ONLY (their bodies live in BrnStuntModeScoring.cpp) except the trivial
// 1-3 line getters, which are bodied inline. Signatures (return type / const / params) are
// taken from the DWARF. The action / output-interface parameter types are used only BY
// POINTER, so forward declarations suffice.
//
// EMBED-BY-VALUE rule: ScoringSystem names members + calls methods; it does NOT depend on a
// byte-exact sizeof. NOT byte-verified. Single owner -- grow this slice, do not fork.

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector3, CgsID (typedef u64)
#include "GameSource/GameState/BrnGameStateTypes.h"           // BrnGameState::EStuntType
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"  // CgsContainers::FixedRingBuffer<T,N>
#include "GameShared/GameClasses/Containers/CgsSet.h"         // Set<T,N>

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

        // Trivial direct-member getters -- bodied inline (return type matches the member type).
        s32        GetCurrentScore() const    { return miCurrentScore; }                // :157
        s32        GetTargetScore() const     { return miTargetScore; }                 // :160
        s32        GetComboScore() const;                                               // :163 (s32 from f32 mfComboScore -- conversion lives in .cpp)
        s32        GetComboMultiplier() const { return miComboMultiplier; }             // :166
        u32        GetCurrentStunts() const;                                            // :170 (X360 0x82310640 -- real body)
        u32        GetAllStuntTypesForInProgressStunt() const;                          // :174
        bool       HasTargetScoreBeenExceeded() const;                                  // :177
        bool       IsComboInProgress() const  { return mbComboInProgress; }             // :180 (X360 0x82313510 -- one-line getter)
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
        // GROWN signature (additive, own home): the DWARF (BrnStuntModeScoring.h:214) spelt this
        // with two out-params, but the X360 body (0x823132D0) writes THREE: the combo score
        // (s32*), the "is this a valid/qualifying combo" flag (bool*, derived from miCurrentScore
        // @+0x10 per the asm -- NOT mfComboScore), and a third f32* it fills from mfPendingScoreTimer.
        // We trust the asm per the
        // asm-overrides-DWARF rule and add lpComboTimer so the recovered body matches its
        // declaration. The sole caller (HUDMessageLogic::GenerateStuntMessage) is not yet done,
        // so growing the arity here cannot break a committed embedder.
        bool       WasComboRecentlyPerformed(s32* lpScore, bool* lpValid, f32* lpComboTimer); // :214 (grown +lpComboTimer per X360 asm)
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

        // --- LEDGER-ONLY helpers (no DWARF signature) ----------------------------------
        // The ledger lists 6 StuntModeScoring methods that the DecFIGS DWARF dropped (inlined
        // / no out-of-line emit in the dumped TU): the DWARF struct body (BrnStuntModeScoring.h
        // lines 117-311) does NOT declare them, so return types + parameter lists below are
        // BEST-EFFORT, inferred from the method name + the sibling DWARF helpers they mirror.
        // They are private members called only from StuntModeScoring's own .cpp -- no embedder
        // (ScoringSystem) names them, so the exact signature does not affect the embedder gate;
        // their bodies + final signatures land with this type's TU. FLAG: re-confirm against the
        // X360 asm when BrnStuntModeScoring.cpp is reconstructed.
        void       BankMultiplier();                                                    // X360 0x82312D68 (best-effort)
        s32        CalculateMultiplier();                                               // X360 0x82312DE8 (best-effort; mirrors GetComboMultiplier)
        void       DealWithInProgressStunt(const GameStateModuleIO::WorldStuntAction* lpAction); // X360 0x82321710 (best-effort; mirrors DealWithStunt)
        void       UpdateStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar);     // X360 0x82338908 (best-effort; mirrors UpdateAirStunts driver)
        void       UpdateScores(f32 lfDelta);                                           // X360 0x82338A98 (best-effort; mirrors UpdateBufferedScore)
        void       PreWorldUpdate(const ActiveRaceCarOutputInterface* lpRaceCar, f32 lfDelta);   // X360 0x823446F8 (best-effort)

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

        // Per-category rating state. GROWN to 18 (additive, own home): the X360 bodies for
        // GetCurrentStunts (0x82310640), ShouldBankScore (0x82313208) and OutputStuntsToDisplay
        // (0x823211E8) all iterate mStuntTypeInfo[0..17] -- i.e. they index every EStuntType slot
        // from E_STUNT_TYPE_SPIN (0) through E_STUNT_TYPE_RATING_AWESOME (18-1=17 by the `< 18`
        // loop bound), NOT just the 15 real categories (E_STUNT_TYPE_COUNT). The old [15] was a
        // minimal-slice guess that the recovered asm proves too small; the array spans the full
        // EStuntType index range (categories 0-14 + the error/rating pseudo-types 15-17).
        // KU_STUNT_TYPE_INFO_COUNT = 18.
        StuntTypeInfo mStuntTypeInfo[18];       // :344 (was [15]; grown per X360 asm loop bounds)

        // --- container members (DWARF declared order + types) ---
        // The recent-jump ring buffer (KI_MAX_RECENT_JUMPS = 256 Vector3 entries). DWARF
        // typedef BrnStuntModeScoring.h:68 RecentJumpSet == FixedRingBuffer<Vector3,256>.
        // Cleared in ClearData/EndCombo, Constructed in Construct, Push'd in UpdateBufferedScore.
        typedef CgsContainers::FixedRingBuffer<Vector3, 256> RecentJumpSet;              // :68
        RecentJumpSet mRecentJumpSet;           // :346

        // The set of stunt-element CgsIDs already counted this run (KI capacity 512). DWARF
        // typedef BrnStuntModeScoring.h:70 StuntElementSet == Set<CgsID,512u>.
        typedef Set<CgsID, 512> StuntElementSet;                                         // :70
        StuntElementSet mRecentStuntElementSet; // :348

        // The recent-prop ring buffer (KI_MAX_RECENT_PROPS = 64 prop-id u16 entries). DWARF
        // typedef BrnStuntModeScoring.h:69 RecentPropSet == FixedRingBuffer<uint16_t,64>.
        typedef CgsContainers::FixedRingBuffer<u16, 64> RecentPropSet;                   // :69
        RecentPropSet mRecentPropSet;           // :350

        bool          mbEndlessStuntRun;        // :352
        AchievementManager* mpAchievementManager; // :354
    };
}
