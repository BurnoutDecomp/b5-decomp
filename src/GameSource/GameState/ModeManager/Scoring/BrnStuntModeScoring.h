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

#include <cstring>                                            // memcpy / size_t (SetOnlineChainableMultiplierDisplay)
#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector3, CgsID (typedef u64)
#include "GameSource/GameState/BrnGameStateTypes.h"           // BrnGameState::EStuntType
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT (SetOnlineChainableMultiplierDisplay guards)
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"  // CgsContainers::FixedRingBuffer<T,N>
#include "GameShared/GameClasses/Containers/CgsSet.h"         // Set<T,N>

// PreWorldUpdate's reconciled (X360-proven) signature names the per-frame output-action queue the
// scorer drains its pending stunt-info HUD event into: CgsModule::VariableEventQueue<13312,16>* (see
// the PreWorldUpdate reconciliation note below). Forward-declared here (pointer-only in the decl);
// the full template definition is pulled in by BrnStuntModeScoring_UpdatePass.cpp where AddEvent is
// actually called.
namespace CgsModule
{
    template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue;
}

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
        // Used by pointer only in DealWithInProgressStunt (reconciled signature). The in-progress
        // stunt-element action the X360 body (0x82321710) dereferences for its convoy-shaped record;
        // homed lean in BrnGameActions.h (see the DealWithInProgressStunt reconciliation note below).
        struct OnStuntElementCompleteAction;
    }

    // DWARF BrnStuntModeScoring.h:36 -- the achievement-manager member is a pointer to this
    // (typedef AchievementManager == BrnGameState::AchievementManagerPS3). Pointer only.
    class AchievementManagerPS3;

    // ------------------------------------------------------------------------
    // TidyStuntScore -- free function the stunt scorer uses to "tidy" a raw stunt score into the
    // clean (integer-valued) score it banks / displays. DWARF homes the DECLARATION in this TU
    // (BrnStuntModeScoring.cpp:88: `extern float32_t TidyStuntScore(float32_t)`); the DEFINITION is
    // a thin rw::math::fpu wrapper the build inlined into this TU (DecFIGS func map: dominant file
    // SDKs/EATech/include/rw/math/fpu/scalar.h, inlined into BrnStuntModeScoring.cpp). It is called
    // by UpdateScores -> UpdateBufferedScore (0x8232C118, tidies mfComboScore before banking) and by
    // OutputStuntsToDisplay (0x823211E8, tidies each surfaced category score). Because UpdateScore /
    // UpdateScores / OutputStuntsToDisplay live in StuntModeScoring's own TU and call it, the
    // declaration belongs in this home so every partial sees one prototype.
    //
    // We declare it here and give it a BEST-EFFORT inline body: round the raw score DOWN to an
    // integer value (the call sites all cast the result to s32 / store it as the displayed score, and
    // UpdateBufferedScore's sibling spin/roll counters use the X360 `vrfim` floor idiom). The exact
    // X360 rounding (0x82312CD0) is UNRECOVERED -- magnitude not load-bearing for the embedder gate.
    // FLAG: confirm the precise rounding (floor vs round-to-nearest vs clamp) against the asm when
    // BrnStuntModeScoring.cpp's full TU is reconstructed; if it proves to be a shared rw::math helper
    // it should be re-homed to its rw/math header and this inline removed.
    inline f32 TidyStuntScore(f32 lfScore)
    {
        // Floor to an integer-valued score (best-effort; see FLAG above).
        return static_cast<f32>(static_cast<s32>(lfScore));
    }

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
        // ScoringSystem embeds this scorer by value (mStuntModeScoring / mOnlineStuntModeScoring) and
        // legitimately drives its per-frame pre-world step: ScoringSystem::UpdateOnlineStuntModeScorePreWorld
        // (X360 0x8234CE48) calls mOnlineStuntModeScoring.PreWorldUpdate(queue) directly (the X360 xref
        // proves the call). PreWorldUpdate is `protected` (it is normally reached through the per-mode
        // update driver), so grant ScoringSystem friendship for faithful access WITHOUT changing the
        // method's signature or section.
        friend class ScoringSystem;

    public:
        // --------------------------------------------------------------------
        // MultiplierOutInfo -- the 24-byte (0x18) output record CalculateMultiplier
        // (X360 0x82312DE8) fills on the stack and BankMultiplier (X360 0x82312D68)
        // hands it. NO DWARF home: the DecFIGS dwarfdump for this TU dropped both the
        // helper signatures and this output type, so the field set + layout below are
        // X360-PROVEN (the store-for-store decode of CalculateMultiplier), provisionally
        // named per CXX_NAMING_CONVENTIONS where the DWARF is silent.
        //
        // Byte offsets CalculateMultiplier writes (relative to the 24-byte struct base):
        //   +0  u16  -- set to lpStuntInfo->muFlatSpins   when (awesome & 0x01)
        //   +2  u16  -- set to rol16(muBarrelRolls,1)      when (awesome & 0x02)
        //   +6  bool -- set to 1                           when (awesome & 0x04)
        //   +7  bool -- set to 1                           when (awesome & 0x10)
        //   +8  bool -- set to 1                           when (awesome & 0x40)
        // The struct is 0x18 (24) bytes; bytes 9..0x17 are unwritten scratch/padding in
        // the recovered body. We model the proven fields by name + an explicit trailing
        // pad so the size matches the stack reservation the asm makes. NOT byte-verified;
        // semantic-parity slice. FLAG: re-confirm field names/extent when this TU's full
        // BankMultiplier/CalculateMultiplier bodies are reconstructed.
        struct MultiplierOutInfo
        {
            u16  muFlatSpins;            // +0x00 (written when awesome bit 0x01 set)
            u16  muBarrelRolls;          // +0x02 (written when awesome bit 0x02 set)
            u8   mPad0[2];               // +0x04 (unwritten scratch)
            bool mbAwesomeFlatSpin;      // +0x06 (awesome bit 0x04)
            bool mbAwesomeBarrelRoll;    // +0x07 (awesome bit 0x10)
            bool mbAwesomeStunt;         // +0x08 (awesome bit 0x40)
            u8   mPad1[15];              // +0x09 (pads the record out to 24 bytes)
        };

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

        // ADDITIVE GROW (declare-only) for the BrnGameState::DeveloperChallengeManager TU.
        // OnEventWin (stunt run) reads a score word at scorer+456 for the multiplier-driven win
        // challenge (E_DEV_CHALLENGE_EVENT_WIN_STUNT). Body + the real member land with the
        // StuntModeScoring TU. FLAG: offset +456, semantics inferred from the OnEventWin asm.
        s32        GetBestStuntScore() const;
        u32        GetCurrentStunts() const;                                            // :170 (X360 0x82310640 -- real body)
        u32        GetAllStuntTypesForInProgressStunt() const;                          // :174
        bool       HasTargetScoreBeenExceeded() const;                                  // :177
        bool       IsComboInProgress() const  { return mbComboInProgress; }             // :180 (X360 0x82313510 -- one-line getter)
        bool       IsComboWarningActive() const;                                        // :183
        f32        GetTimeSinceComboWarningActivated() const;                           // :187

        // --- combo-timer named access (asm-proven role of two committed members) ---------
        // The combo machinery (BeginCombo 0x82313428 zeros both; UpdateCombo 0x82320FF0
        // accumulates both by frame delta; EndCombo 0x823215D8 copies the warning timer out)
        // touches two f32 timers at this+0x58 and this+0x5C. Reconciling the X360 byte offsets
        // against the committed DWARF member ORDER, those two slots ARE the committed members
        // mfTimeSinceLastStunt (+0x58) and mfSpeedMPHBeforeCrashing (+0x5C): the DWARF spelt them
        // with those source-level identifiers, but the recovered asm proves their RUNTIME role is
        // the combo timers (a frame-delta accumulator cannot be a "speed before crashing"). Per the
        // additive-grow rule we do NOT rename/retype/reorder the committed members; instead we
        // publish combo-timer-named accessors that ALIAS them, so BeginCombo/UpdateCombo/EndCombo
        // get clean named access without disturbing the layout the embedders depend on.
        //   mfTimeSinceLastStunt    (+0x58): UpdateCombo grows it once per combo-no-stunt frame;
        //                                    crossing KF_TIME_WITHOUT_STUNT_TO_LOSE_COMBO ends the combo.
        //   mfSpeedMPHBeforeCrashing(+0x5C): the per-frame combo-active timer UpdateCombo grows
        //                                    unconditionally; EndCombo snapshots it into the
        //                                    recent-combo time @+0x88; BeginCombo zeros it.
        // FLAG: provisional role mapping (offset X360-proven); confirm the canonical combo-timer
        // member names when BrnStuntModeScoring.cpp's full TU lands.
        f32        GetTimeSinceLastScoringStunt() const  { return mfTimeSinceLastStunt; }      // +0x58
        void       SetTimeSinceLastScoringStunt(f32 lfTime) { mfTimeSinceLastStunt = lfTime; }
        f32        GetComboActiveTimer() const           { return mfSpeedMPHBeforeCrashing; }  // +0x5C
        void       SetComboActiveTimer(f32 lfTime)       { mfSpeedMPHBeforeCrashing = lfTime; }
        // The recent-combo snapshot timer (this+0x88) -- backed by mfRecentComboTime (added below).
        f32        GetRecentComboTime() const            { return mfRecentComboTime; }         // +0x88
        void       SetRecentComboTime(f32 lfTime)        { mfRecentComboTime = lfTime; }

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
        // @+0x10 per the asm -- NOT mfComboScore), and a third f32* it fills from mfRecentComboTime
        // (+0x88, `lfs 0x88` -- NOT mfPendingScoreTimer@+0x8C). We trust the asm per the
        // asm-overrides-DWARF rule and add lpComboTimer so the recovered body matches its
        // declaration. The sole caller (HUDMessageLogic::GenerateStuntMessage) is not yet done,
        // so growing the arity here cannot break a committed embedder.
        bool       WasComboRecentlyPerformed(s32* lpScore, bool* lpValid, f32* lpComboTimer); // :214 (grown +lpComboTimer per X360 asm)
        bool       WasTimeRecentlyUp();                                                 // :218

    protected:
        // ===== ADDITIVE (flagged) protected accessors for the derived online scorer =====
        // BrnGameState::StuntModeScoringOnline derives from this class and its store-for-store overrides
        // (EndCombo/Update/ClearData/BeginCombo) read+write four committed base scalars the base keeps
        // private: miCurrentScore (+0x10), mfComboScore (+0x20), miComboMultiplier (+0x24) and
        // mbStuntInProgress (+0x28). Rather than make those data members protected (which would touch the
        // committed layout / embedder expectations), expose minimal protected accessors that ALIAS them.
        // Purely additive (no member added/reordered/retyped). FLAG: shared-home grow.
        s32  GetCurrentScoreInternal() const           { return miCurrentScore; }          // +0x10
        void SetCurrentScoreInternal(s32 liScore)      { miCurrentScore = liScore; }
        s32  GetComboScoreAsInt() const                { return static_cast<s32>(mfComboScore); } // (s32)+0x20
        s32  GetComboMultiplierInternal() const        { return miComboMultiplier; }        // +0x24
        void SetComboMultiplierInternal(s32 liMult)    { miComboMultiplier = liMult; }
        bool IsStuntInProgressInternal() const         { return mbStuntInProgress; }        // +0x28
        // The cached online display score = (s32)mfComboScore * miComboMultiplier + miCurrentScore.
        s32  ComputeOnlineDisplayScore() const
        {
            return GetComboScoreAsInt() * miComboMultiplier + miCurrentScore;
        }

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
        //
        // BankMultiplier / CalculateMultiplier signatures GROWN here (additive, own home) to the
        // X360-proven shapes -- the round-1 store-for-store decode (.wf/gm-stuntmode-layout/
        // bodies-draft/BrnStuntModeScoring_Combo.cpp) proves:
        //   s32 BankMultiplier(StuntInfo* lpRecentStunt)      -- 0x82312D68; asserts lpRecentStunt,
        //       calls CalculateMultiplier into a stack MultiplierOutInfo, writes
        //       lpRecentStunt->miStuntMultiplier, returns the multiplier.
        //   s32 CalculateMultiplier(const StuntInfo*, MultiplierOutInfo*) -- 0x82312DE8; popcount of
        //       the awesome-type mask + flat-spin/barrel-roll contributions, filling the out record.
        // Both are virtual in the original (the online variant overrides; base dispatched via vtable
        // +0x28). We declare CalculateMultiplier virtual so the override can bind; the full vtable
        // ORDER is still deferred (see the HasStuntModeEnded note above) -- declaring this one extra
        // virtual is enough for the body + the dispatch to compile (semantic parity; no layout assert).
        s32        BankMultiplier(StuntInfo* lpRecentStunt);                            // X360 0x82312D68 (grown per asm)
        virtual s32 CalculateMultiplier(const StuntInfo* lpStuntInfo,
                                        MultiplierOutInfo* lpMultiplierOutInfo);        // X360 0x82312DE8 (grown per asm; virtual)

        // RECONCILED signature (was best-effort `void(const WorldStuntAction*)`). The X360 call-site
        // ModeManager::ProcessEvent (0x82340AB8) proves the real args: r4 = the in-progress
        // stunt-element action pointer, f1 = the frame delta (f32), r6 = the player's active-race-car
        // index (GameStateModule::GetPlayerActiveRaceCarIndex, used as a plain s32 in the convoy id
        // search). IDA's a4(r5) is uninitialised garbage at the call site (never loaded) -> dropped.
        // The action type is the convoy-shaped OnStuntElementCompleteAction (the action behind
        // E_ACTION_ON_STUNT_ELEMENT_COMPLETE the body walks at +0x24/+0x44/+0x64), NOT WorldStuntAction.
        // Player index typed s32 (not EActiveRaceCarIndex) to avoid pulling the race-car-interface
        // enum into this keystone; the asm compares it as a 32-bit word against the convoy id array.
        void       DealWithInProgressStunt(const GameStateModuleIO::OnStuntElementCompleteAction* lpAction,
                                           f32 lfDelta, s32 liPlayerActiveRaceCarIndex);  // X360 0x82321710 (reconciled; call-site 0x82340AB8 proved arg count/types)
        void       UpdateStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar);     // X360 0x82338908 (best-effort; mirrors UpdateAirStunts driver)

        // RECONCILED signature (was best-effort `void(f32)`). The X360 body (0x82338A98) gates ALL its
        // work behind a player-active test that dereferences the output interface (*(a2+10328) ==
        // GetPlayerActiveRaceCarIndex asserted < count, *(a2+10336) == IsPlayerCarActive); only when
        // active does it call UpdateCombo/UpdateBufferedScore/UpdateStuntRepetition. The Update
        // call-site (0x82340BC0) loads r4 = the interface, f1 = delta -> sig carries the interface.
        // Param ORDER matches the sibling UpdateStunts(f32, interface*) so Update's call site is uniform.
        void       UpdateScores(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar);     // X360 0x82338A98 (reconciled; call-site 0x82340B40 proved the interface arg)

        // RECONCILED signature (was best-effort `void(const ActiveRaceCarOutputInterface*, f32)`).
        // The X360 body (0x823446F8) takes NO interface and NO float: r3 = this, r4 = the per-frame
        // output-action queue (CgsModule::VariableEventQueue<13312,16>*). When mbStuntInfoEventPending
        // (+0x22BD) is set it packs an 8-byte payload {u32 mRecentStunt.muAwesomeStuntTypes; s16
        // muFlatSpins; s16 muBarrelRolls} and calls queue->AddEvent(&payload, 21, 8) (asserting the
        // queue non-null), then clears the flag. Both call-sites (ScoringSystem::
        // UpdateOnlineStuntModeScorePreWorld 0x8234CE48, ModeManager::PreWorldUpdate 0x823537B8) thread
        // the queue in r4 -- no interface, no delta.
        void       PreWorldUpdate(CgsModule::VariableEventQueue<13312, 16>* lpOutputActionQueue);  // X360 0x823446F8 (reconciled; AddEvent-queue arg, no interface/delta)

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

        // GROWN (additive, own home): the f32 the X360 build keeps at this+0x88, between
        // miRecentComboScore (+0x84) and mfPendingScoreTimer (+0x8C). The DecFIGS DWARF dropped it
        // (its member list jumps 0x84 -> 0x8C), but the asm proves it: EndCombo (0x823215D8) writes
        //   *(this+0x88) = *(this+0x5C)   -- snapshots the combo-active timer at combo end
        // and WasComboRecentlyPerformed (0x823132D0) reads *(this+0x88) out to its f32* out-param.
        // Provisionally named mfRecentComboTime (offset X360-proven; the recent-combo timer surfaced
        // alongside mbRecentCombo/miRecentComboScore). NOT in DWARF -> FLAG name on full-TU landing.
        f32           mfRecentComboTime;        // +0x88 (X360-proven; DWARF-silent)

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

        // GROWN (additive, own home): the per-stunt-type s32 hit-counter array the X360 build keeps
        // at this+0x1B0, immediately after mStuntTypeInfo[18] (which spans 0x90..0x1AF at 16-byte
        // stride). The DecFIGS DWARF dropped it. The asm proves it:
        //   UpdateScore (0x823212D8): ++LODWORD(this[a4 + 0x6C/4])  i.e. ++maStuntTypeScoreCount[a4]
        //                             where a4 is the EStuntType (0..17) -- byte 0x1B0 + 4*a4.
        //   ClearData   (0x82321108): the second loop (mtctr 18, stw 0, base this+0x1B0, stride 4)
        //                             zeros all 18 entries.
        // It records how many times each stunt category has scored this run. 18 wide, matching the
        // EStuntType index range (0..E_STUNT_TYPE_RATING_AWESOME-1). Provisionally named
        // maStuntTypeScoreCount (offset X360-proven; DWARF-silent). FLAG: confirm name on full-TU land.
        s32           maStuntTypeScoreCount[18]; // +0x1B0 (X360-proven; DWARF-silent)

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

        // GROWN (additive, own home): the "a stunt-info summary event is queued for the HUD" flag the
        // X360 build keeps at this+0x22BD, between mbEndlessStuntRun (+0x22BC) and the pointer member.
        // The DecFIGS DWARF dropped it. The asm proves it:
        //   UpdateBufferedScore (0x8232C118): *(this+0x22BD) = (mRecentStunt.muAwesomeStuntTypes != 0)
        //                                     -- arms the pending flag when a stunt is banked.
        //   PreWorldUpdate      (0x823446F8): reads the flag; when set, packs the recent-stunt summary
        //                                     into the VariableEventQueue and CLEARS it.
        //   ClearData           (0x82321108): zeros it.
        // Provisionally named mbStuntInfoEventPending (offset X360-proven; DWARF-silent). FLAG: confirm
        // name on full-TU landing.
        bool          mbStuntInfoEventPending;  // +0x22BD (X360-proven; DWARF-silent)

        AchievementManager* mpAchievementManager; // :354

    public:
        // Named access for the X360-proven, DWARF-silent stunt-info-event-pending flag (+0x22BD).
        // UpdateBufferedScore arms it; PreWorldUpdate reads+clears it. Public so PreWorldUpdate's
        // sibling-mode caller chain (deferred TUs) can consume it by name.
        bool GetStuntInfoEventPending() const    { return mbStuntInfoEventPending; }
        void SetStuntInfoEventPending(bool lbOn) { mbStuntInfoEventPending = lbOn; }

        // ===== Online chainable-multiplier hand-off surface (X360-proven; DWARF-silent) =====
        //
        // ScoringSystem::UpdateOnlineStuntModeScorePreWorld (X360 0x8234CE48), after gathering the
        // per-car chainable-multiplier table, gates BOTH the PreWorldUpdate call AND the table hand-off
        // behind a flag the X360 build keeps inside this scorer at this+0x2515 (asm 0x8234CF68
        // lbz 0x2515(this+0x2620 base)), then memcpy's the 132-byte gathered table into this scorer's
        // internal display buffer at this+0x23F0 (asm 0x8234CF80 addi 0x23F0 / memcpy size 0x84).
        //
        // Both slots are DWARF-silent StuntModeScoring internals; model them as additive named members
        // (this slice is accessed BY NAME, never sized -- no layout assert). The 132-byte display buffer
        // is taken/handed by raw bytes + size so this header need not pull the CarScoreData /
        // ChainableMultiplierInfo type in (that lives in BrnGameStateSharedIO.h, which would cycle).
        // Provisional names (offsets X360-proven); FLAG: re-confirm when this type's full TU lands.
        bool IsOnlineChainableUpdateSuppressed() const { return mbOnlineChainableUpdateSuppressed; }     // +0x2515
        void SetOnlineChainableUpdateSuppressed(bool lbSuppressed) { mbOnlineChainableUpdateSuppressed = lbSuppressed; }

        // Receive the gathered chainable-multiplier display table (the X360 memcpy into +0x23F0). Inline
        // so callers need no out-of-line StuntModeScoring TU; reproduces the byte-for-byte X360 memcpy.
        static const s32 KI_ONLINE_CHAINABLE_DISPLAY_BYTES = 132; // X360 memcpy size 0x84
        void SetOnlineChainableMultiplierDisplay(const void* lpData, s32 liSizeInBytes)                  // +0x23F0
        {
            CGS_ASSERT(lpData != NULL, "lpData");
            CGS_ASSERT(liSizeInBytes <= KI_ONLINE_CHAINABLE_DISPLAY_BYTES, "liSizeInBytes <= KI_ONLINE_CHAINABLE_DISPLAY_BYTES");
            memcpy(maOnlineChainableMultiplierDisplay, lpData, static_cast<size_t>(liSizeInBytes));
        }

    private:
        // +0x2515 -- gate flag (X360-proven; DWARF-silent). When set, UpdateOnlineStuntModeScorePreWorld
        // skips driving PreWorldUpdate + the display copy.
        bool mbOnlineChainableUpdateSuppressed;
        // +0x23F0 -- the 132-byte online chainable-multiplier display buffer
        // (Array<CarScoreData::ChainableMultiplierInfo,8> raw image; modeled as bytes to avoid a header cycle).
        u8   maOnlineChainableMultiplierDisplay[KI_ONLINE_CHAINABLE_DISPLAY_BYTES];
    };
}
