#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                      // CgsID (u64), Vector3
#include "GameSource/BurnoutConstants.h"                         // EActiveRaceCarIndex, BrnGameState::EChallengeStatus
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"   // CgsContainers::FastBitArray<N>
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"     // CgsContainers::ObjectPool<T,N,TIndex>
#include "GameShared/GameClasses/Containers/CgsArray.h"          // Array<T,N> (global scope)
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (EFreeburnSkill operator++)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"      // BrnNetwork::NetworkPlayerID (s32)
#include "GameSource/GameState/BrnGameStateSharedIO.h"           // GameStateModuleIO::{EFreeburnChallengeSuccess, CompletedFburnChallengesData}
#include "GameSource/GameState/BrnGameEvents.h"                  // GameStateModuleIO::{EGameEventType, FburnChallengeSuccessUpdateEvent, FburnChallengeSuccessEvent}
#include "GameSource/GameState/BrnGameStateTypes.h"              // BrnGameState::StuntElementType
#include "SharedClasses/DataLists/ChallengeListEntry.h"          // BrnResource::{ChallengeListEntry, ChallengeListEntryAction}
#include "SharedClasses/StreetData/BrnChallengeData.h"           // BrnStreetData::{ChallengePlayerScoreEntry, ScoreType}
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"  // embedded by value

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h
// ============================================================================
// OWNING HEADER for BrnGameState::ChallengeManager -- the top-level CHALLENGE-mode
// manager that owns + drives the online/freeburn challenge subsystem (challenge tracking,
// per-player skill scoring, arbitration, results, network player book-keeping).
//
// LAYOUT MODEL -- NAMED MEMBERS (keystone freeze; replaces the earlier opaque
// maOpaque[0x1020] foundation slice):
//   Member NAMES + ORDER are DWARF-authoritative (DecFIGS BrnChallengeManager.h:381-453,
//   :717-720). Every byte offset is X360-asm-attested (Construct 0x82332DB0 / Destruct
//   0x8233A9C0 / Prepare 0x8233AAF8 / ClearPlayerSuccessData 0x82323158 /
//   ClearFreeburnSkillsThisFrame 0x82323498 sweep the whole object by offset; the
//   per-member X360 offsets are annotated below). The X360 object spans 0x1020 bytes.
//
//   X360-vs-PS3-DWARF DRIFT (all attested, see per-member notes):
//     * EFreeburnSkill has COUNT == 38 on X360 (PS3 DWARF: 19). Every per-skill array is
//       [38] (loop bounds `cmpwi 0x26`, SetCurrentSkillScore asserts "< E_FREEBURN_SKILL_COUNT"
//       against 38, FastBitArray<38> range assert prints "max bits: 38"). The PS3 enumerator
//       NAMES (19 of them) are kept verbatim; the drifted 19..37 stunt-skill ids have no
//       recoverable names and are used as attested numeric casts where the asm does.
//     * Construct takes FIVE injected managers (PS3 DWARF: four) -- lpModeManager is the
//       X360-only 2nd parameter, cached in the X360-only member mpModeManager (@+0xE4C).
//     * miLastChallengeResetFrame (@+0x1000) is X360-only (init -1; latched to
//       miFramesSinceNetworkStart by the challenge-reset ProcessEvent cases; success events
//       older than it are ignored). Name reconstructed from that attested behaviour.
//
//   POINTER-INVARIANCE: everything BEFORE mpVehicleList (@+0x5C0) is pointer-free, so the
//   x64 gate reproduces the X360 offsets absolutely there (pinned in _AssertLayout).
//   From mpVehicleList on, x64 pointers are 8 bytes and offsets drift; only relative
//   anchors within pointer-free runs are pinned.
// ============================================================================

namespace BrnResource
{
    class  ChallengeList;      // mpFreeburnChallengeList; entry count @+0x32E0, GetChallengeData/GetChallengeIndex
    class  VehicleList;        // mpVehicleList (CheckCurrentCar)
}
namespace BrnProgression { class ProgressionManager; }
namespace CgsSystem       { class TimerStatusInterface; }
namespace CgsModule
{
    struct Event;   // shared event base (defined in CgsVariableEventQueue.h)
    template <typename T> class BaseEventQueue;
    template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue;
}
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnGameState
{
    class ModeManager;         // X360-only Construct arg / mpModeManager back-pointer
    class GameStateModule;
    class RoadRulesManager;
    class TriggerQueryManager;
    namespace GameStateModuleIO
    {
        struct OutputBuffer;                    // full def: BrnGameStateModuleIO.h
        struct FreeburnChallengeUpdateAction;   // full def: BrnGameActions.h
    }
    // DWARF spells the crash-event queue "VehicleManagerOutputInterface::RaceCarCrashEventQueue"
    // (same fwd-namespace pattern as the committed BrnScoringSystem.h).
    namespace VehicleManagerOutputInterface { struct RaceCarCrashEventQueue; }

    // ------------------------------------------------------------------------
    // Nested enums (DWARF BrnChallengeManager.h:53 / :63). These are the type's own; not
    // defined anywhere else in the committed tree.
    // ------------------------------------------------------------------------

    // BrnChallengeManager.h:53 -- the manager's top-level state machine.
    enum EChallengeManagerStatus
    {
        E_CHALLENGE_MANAGER_STATUS_NONE    = 0,
        E_CHALLENGE_MANAGER_STATUS_PENDING = 1,
        E_CHALLENGE_MANAGER_STATUS_RUNNING = 2,
        E_CHALLENGE_MANAGER_STATUS_RESULTS = 3,
        E_CHALLENGE_MANAGER_STATUS_COUNT   = 4,
    };

    // BrnChallengeManager.h:63 -- the freeburn skill categories a challenge action can score.
    // NOTE: START and ONCOMING alias 0 in the DWARF (the enumerator order is preserved verbatim).
    // X360 DRIFT: the X360 build's enum carries 19 additional stunt-run skills (ids 19..37,
    // E_FREEBURN_SKILL_COUNT == 38 there -- see KI_FREEBURN_SKILL_COUNT_X360). Their names are
    // not recoverable; the PS3-attested names below are kept verbatim.
    enum EFreeburnSkill
    {
        E_FREEBURN_SKILL_START                       = 0,
        E_FREEBURN_SKILL_ONCOMING                    = 0,
        E_FREEBURN_SKILL_FLATSPIN                    = 1,
        E_FREEBURN_SKILL_FLATSPIN_REVERSE            = 2,
        E_FREEBURN_SKILL_DRIFT                       = 3,
        E_FREEBURN_SKILL_NEAR_MISS                   = 4,
        E_FREEBURN_SKILL_SUCCESSFUL_LANDING          = 5,
        E_FREEBURN_SKILL_SUCCESSFUL_LANDING_REVERSE  = 6,
        E_FREEBURN_SKILL_ROAD_RULE_TIME              = 7,
        E_FREEBURN_SKILL_ROAD_RULE_CRASH             = 8,
        E_FREEBURN_SKILL_BARREL_ROLL                 = 9,
        E_FREEBURN_SKILL_BARREL_ROLL_REVERSE         = 10,
        E_FREEBURN_SKILL_PLAYER_POWER_PARKING        = 11,
        E_FREEBURN_SKILL_TRAFFIC_POWER_PARKING       = 12,
        E_FREEBURN_SKILL_BURNOUTS                    = 13,
        E_FREEBURN_SKILL_BOOST_TIME                  = 14,
        E_FREEBURN_SKILL_AIR                         = 15,
        E_FREEBURN_SKILL_AIR_DISTANCE                = 16,
        E_FREEBURN_SKILL_BILLBOARDS                  = 17,
        E_FREEBURN_SKILL_LEAP_CARS                   = 18,
        E_FREEBURN_SKILL_COUNT                       = 19,
    };

    // The X360 build's E_FREEBURN_SKILL_COUNT (drifted enum; see EFreeburnSkill note). Every
    // per-skill array/loop in this TU is bounded by THIS value on X360 (`cmpwi 0x26` == 38).
    static const s32 KI_FREEBURN_SKILL_COUNT_X360 = 38;

    // The X360 build's ChallengeListEntryAction::E_ACTION_COUNT (drifted enum: the committed
    // ChallengeListEntry.h spells the 22 PS3-named action types; the X360 build appends 19
    // stunt-run action types 22..40 -- see KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL below,
    // whose 41 entries are that full drifted range). UpdateAction's action-type range assert
    // is `cmplwi 0x29` (41) on X360 -- bound it with THIS constant, not E_ACTION_COUNT.
    static const s32 KI_CHALLENGE_ACTION_TYPE_COUNT_X360 = 41;

    // DWARF BrnChallengeManager.h:89 -- the post-increment the per-skill loops use
    // (`for (skill = E_FREEBURN_SKILL_START; skill < COUNT; skill++)`). The X360 build inlines
    // it everywhere: bump, then assert the new value has not run past the (X360, 38) count.
    // The assert string + file/line are verbatim from the X360 asm (BrnChallengeManager.h:113).
    inline EFreeburnSkill operator++(EFreeburnSkill& lreSkill, int)
    {
        const EFreeburnSkill leOld = lreSkill;
        lreSkill = static_cast<EFreeburnSkill>(static_cast<s32>(lreSkill) + 1);
        CGS_ASSERT(static_cast<s32>(lreSkill) <= KI_FREEBURN_SKILL_COUNT_X360,
                   "leEnumIndex <= E_FREEBURN_SKILL_COUNT");
        return leOld;
    }

    // ------------------------------------------------------------------------
    // X360-recovered rodata tables shared by several methods of this TU (values dumped from
    // BURNOUT_X360_ARTIST.XEX -- big-endian -- at the annotated addresses; NOT invented).
    // Internal linkage per including TU; the partfile bodies read them by name.
    // ------------------------------------------------------------------------

    // X360 dword_82021288 (41 x s32). EChallengeActionType -> EFreeburnSkill map used by
    // UpdateActionSuccess / UpdateChallenge / WriteDataToOutputForTarget / ResetActionData.
    // 38 (== KI_FREEBURN_SKILL_COUNT_X360) is the "no skill for this action type" sentinel.
    // The PS3-named action types 0..21 map onto the PS3-named skills (MINIMUM_SPEED, LEAP_CARS,
    // CRASH_INTO_PLAYER and MEET_UP are the sentinel entries); 22..40 are the X360-drift
    // stunt-run action types mapping onto the drifted skills 19..37.
    static const s32 KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL[41] =
    {
        38, 15, 16, 38,  3,  4,  9,  0,  1,  5,   //  0..9   (MINIMUM_SPEED..LAND_SUCCESSFUL)
         7,  8, 11, 12, 38, 13, 38, 17, 14, 10,   // 10..19  (ROAD_RULE_TIME..BARREL_ROLLS_REVERSE)
         2,  6, 19, 20, 21, 22, 23, 24, 25, 26,   // 20..29  (FLATSPIN_REVERSE, LAND_SUCCESSFUL_REVERSE, drift block...)
        27, 28, 29, 30, 31, 32, 33, 34, 35, 36,   // 30..39
        37                                        // 40
    };

    // X360 byte_8202132C (38 x bool). Per-skill "score is a CONTINUOUS value" flag: gates
    // SetCurrentSkillScore's cache-when-location-not-ok path and its ScoresSetThisFrame bit.
    // Set for ONCOMING(0), DRIFT(3), NEAR_MISS(4), BURNOUTS(13), BOOST_TIME(14), AIR(15),
    // AIR_DISTANCE(16) and drift skills 20 + 36. (Name reconstructed; values attested.)
    static const u8 KAB_FREEBURN_SKILL_IS_CONTINUOUS[KI_FREEBURN_SKILL_COUNT_X360] =
    {
        1,0,0,1,1,0,0,0,0,0, 0,0,0,1,1,1,1,0,0,0, 1,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,1,0
    };

    // X360 byte_82021354 (38 x bool). Per-skill "bank the running value when the player leaves
    // the action's location" flag: UpdateLocationOKStatusChange banks mafActiveSkillValue[skill]
    // for these skills on an ok->not-ok transition. Same set as IS_CONTINUOUS minus AIR/
    // AIR_DISTANCE. (Name reconstructed; values attested.)
    static const u8 KAB_FREEBURN_SKILL_BANK_ON_LOCATION_EXIT[KI_FREEBURN_SKILL_COUNT_X360] =
    {
        1,0,0,1,1,0,0,0,0,0, 0,0,0,1,1,0,0,0,0,0, 1,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,1,0
    };

    // ========================================================================
    // BrnGameState::ChallengeManager (DWARF spells it `struct`, no base class; kept `class`
    // to match the committed thin slice -- for a non-template type the keyword only changes
    // default access). X360 sizeof == 0x1020 (object spans to +0x101E, 16-aligned by the
    // Vector3-bearing leaping-data pool elements).
    // ========================================================================
    class ChallengeManager
    {
    public:
        // --------------------------------------------------------------------
        // Nested leaping-data records (DWARF BrnChallengeManager.h:311/:325), grown from the
        // former 32-byte blobs to the real DWARF-named fields for the UpdateLeaptCars slice
        // (X360 0x82333BB8). 32-byte X360 element stride preserved: EActiveRaceCarIndex @+0x00,
        // Vector3 @+0x10 (the 16-aligned SIMD member pads the enum out to 16 and aligns the
        // element to 16 -- attested by the pool stride 32 and the `stvx128 v0, r3, r17`
        // (r17 == 16) vector stores in UpdateLeaptCars).
        //   CarLeapingData.mPlayerToCar          = carPos - playerPos this frame (rebuilt per
        //                                          frame into a LOCAL pool).
        //   StoredLeapingData.mLeapRadiusEntryVector = the mPlayerToCar snapshot taken when the
        //                                          car first entered the leap radius (persisted
        //                                          in mPotentiallyLeaptCars; a sign flip of the
        //                                          2D dot against the current mPlayerToCar means
        //                                          the player crossed over the car -> a leap).
        // (Moved here from the provisional BrnChallengeManagerLeapingData.h thin slice, which
        // now forwards to this header -- single owner, grown in place.)
        // --------------------------------------------------------------------
        struct alignas(16) CarLeapingData
        {
            EActiveRaceCarIndex mActiveRaceCarIndex;   // +0x00 (DWARF :312)
            Vector3             mPlayerToCar;          // +0x10 (DWARF :313; 16-aligned SIMD)
        };
        struct alignas(16) StoredLeapingData
        {
            EActiveRaceCarIndex mActiveRaceCarIndex;      // +0x00 (DWARF :326)
            Vector3             mLeapRadiusEntryVector;   // +0x10 (DWARF :327; 16-aligned SIMD)
        };

        // DWARF BrnChallengeManager.h:295 -- one remote player's challenge-completion record.
        // X360 stride 0x108: the FastBitArray<2000> completion bit store (== the committed
        // GameStateModuleIO::CompletedFburnChallenges layout) + the player id (-1 == free slot)
        // + the finalised flag. Construct/Destruct attest the -1/-0 init pattern
        // (stw -1,+0x100 / stb 0,+0x104 per 0x108-stride entry).
        struct ChallengeCompletionData
        {
            CgsContainers::FastBitArray<2000> mCompletedChallenges; // +0x000 (DWARF :297, CompletedFburnChallengesData::CompletedFburnChallenges)
            BrnNetwork::NetworkPlayerID       mNetworkPlayerID;     // +0x100 (DWARF :298; -1 == unused slot)
            bool                              mbFinalised;          // +0x104 (DWARF :299)
        };

        static const s32 KI_MAX_CHALLENGE_PLAYERS = 8;   // per-player array bound (== E_ACTIVE_RACE_CAR_INDEX_COUNT)
        static const s32 KI_MAX_CHALLENGE_ACTIONS = 2;   // per-action bound (== ChallengeListEntryAction::KI_MAX_TARGETS_PER_CHALLENGE_ACTION)
        static const s32 KI_MAX_REMOTE_PLAYERS    = 7;   // maChallengeCompletionData slots

        // ---------------- lifecycle (DWARF :112-:141) ----------------
        // X360 0x82332DB0. FIVE injected managers on X360 (lpModeManager is X360-only drift;
        // asserted + cached @+0xE4C like the DWARF-listed four). EXECUTED in the boot trace.
        void Construct(GameStateModule*             lpGameStateModule,
                       ModeManager*                 lpModeManager,
                       BrnProgression::ProgressionManager* lpProgression,
                       const RoadRulesManager*      lpRoadRulesManager,
                       const TriggerQueryManager*   lpTriggerQueryManager);
        // X360 0x8233A9C0.
        void Destruct();
        // X360 0x8233AAF8. Caches the list, counts challenges per player count, resets the pool.
        bool Prepare(const BrnResource::ChallengeList* lpFreeburnChallengeList);

        // X360 0x82353640 (DWARF :135).
        void PreWorldUpdate(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface,
                            s32 liFramesSinceNetworkStart,
                            const CgsModule::BaseEventQueue<GameStateModuleIO::CompletedFburnChallengesData>* lpCompletedChallengeStatusQueue,
                            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                            bool lbIsOnline,
                            GameStateModuleIO::OutputBuffer* lpOutputBuffer);
        // X360 0x8233AC28 (DWARF :141 -- but see drift note). X360-vs-DWARF DRIFT: the PS3 DWARF
        // lists TWO parameters; the X360 body takes a THIRD, middle parameter in r5 (spilled to
        // the home slot at prologue, reloaded at the tail) that is passed STRAIGHT THROUGH to
        // UpdateStuntScores -- the same un-homed stunt-score snapshot that method reads by
        // attested offset (hence const void*, matching UpdateStuntScores' committed shape).
        void PostWorldUpdate(const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashEventQueue,
                             const void* lpStuntScoreInfo,
                             const CgsSystem::TimerStatusInterface* lpTimerStatusInterface);

        // ---------------- begin / trigger / end (DWARF :150-:185) ----------------
        // The "GameActionQueue" every one of these posts to is the GameStateModuleIO output
        // buffer's CgsModule::VariableEventQueue<13312,16> (committed instantiation TU).
        typedef CgsModule::VariableEventQueue<13312, 16> TGameActionQueue;

        // X360 0x823505B8.
        void BeginChallenge(CgsID lChallengeID, TGameActionQueue* lpActionQueue,
                            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                            bool lbIsOnline, bool lbRemote);
        // X360 0x82346DA0.
        void TriggerFreeburnChallenge(CgsID lChallengeID, TGameActionQueue* lpActionQueue, bool lbRemote);
        // X360 0x823506E8.
        void CancelFreeburnChallenge(TGameActionQueue* lpActionQueue);
        // X360 0x8234DE30.
        void EndChallenge(EChallengeStatus leChallengeStatus, TGameActionQueue* lpActionQueue, bool lbIsOnline);
        // X360: RemoteBeginChallenge / RemoteTriggerFreeburnChallenge are NOT in this TU's
        // X360 ledger (other bucket / inlined); declared per DWARF :174/:179.
        void RemoteBeginChallenge(CgsID lChallengeID);
        void RemoteTriggerFreeburnChallenge(CgsID lChallengeID);
        // X360 0x82347090 (DWARF :185).
        void RemoteEndChallenge(TGameActionQueue* lpActionQueue, EChallengeStatus leChallengeStatus);

        // ---------------- queries (DWARF :189-:206) ----------------
        bool IsChallengeActive() const;    // DWARF :189 (not in this TU's X360 ledger)
        bool IsChallengeRunning() const;   // DWARF :193 (not in this TU's X360 ledger)
        // X360 0x82355FA8 (DWARF :196). Style of the active challenge, or NONE unless RUNNING.
        BrnResource::ChallengeListEntry::EFreeburnChallengeStyle GetChallengeStyle() const;
        void GetChallengeDescription(CgsID lChallengeID, char* lpcBuffer, s32 liBufferLength) const; // DWARF :203 (not in this TU's X360 ledger)
        CgsID GetCurrentFreeburnChallengeID();                                                       // DWARF :206 (not in this TU's X360 ledger)

        // ---------------- event handlers (DWARF :212-:270) ----------------
        // X360 0x8233CDE0.
        void HandleSuccessUpdateEvent(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface,
                                      const GameStateModuleIO::FburnChallengeSuccessUpdateEvent* lpEvent);
        // X360 0x82316AE0.
        void HandleChallengeSuccessEvent(const GameStateModuleIO::FburnChallengeSuccessEvent* lpEvent);
        // X360 0x82334D48.
        void HandleRoadRuleScore(BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                                 BrnStreetData::ScoreType leScoreType, CgsID lRoadID);
        void HandleWorldStunt(StuntElementType leStuntElementType, CgsID lStuntID);  // DWARF :230 (not in this TU's X360 ledger)
        // X360 0x82334B00.
        void OnProfileLoaded();
        // X360 0x823240B8 / 0x82347E88 / 0x8234E420 (DWARF :241/:248/:255).
        void NetworkPlayerAdded(BrnNetwork::NetworkPlayerID lPlayerID, TGameActionQueue* lpActionQueue, bool lbIsOnline);
        void NetworkPlayerFinalised(BrnNetwork::NetworkPlayerID lPlayerID, TGameActionQueue* lpActionQueue, bool lbIsOnline);
        void NetworkPlayerRemoved(BrnNetwork::NetworkPlayerID lPlayerID, TGameActionQueue* lpActionQueue, bool lbIsOnline);
        // X360 0x82348080 (DWARF :260).
        void OutputFreeburnChallengeEveryPlayerStatusEvent(TGameActionQueue* lpActionQueue);
        // X360 0x8234E548 (DWARF :265).
        void Disconnected(TGameActionQueue* lpActionQueue);
        // X360 0x8233D6A8 (DWARF :270). Jump-table dispatch on the game-event type.
        void ProcessEvent(GameStateModuleIO::EGameEventType leEventType, const CgsModule::Event* lpEvent);
        // DWARF :274 (not in this TU's X360 ledger).
        void ApplyVehicleList(const BrnResource::VehicleList* lpVehicleList);
        // DWARF :277/:280 (not in this TU's X360 ledger).
        bool ChallengesAreAllOnePlayer();
        bool ChallengesAreAllTwoPlayer();

        // ----- Named accessors the committed ChallengeManagerDebugComponent reaches. -----
        const BrnResource::ChallengeList*    GetFreeburnChallengeList() const { return mpFreeburnChallengeList; }
        BrnProgression::ProgressionManager*  GetProgressionManager()          { return mpProgression; }
        CgsContainers::FastBitArray<2000>&   GetLocalChallengeCompletionData(){ return mLocalChallengeCompletionData; }

    private:
        friend class ChallengeManagerDebugComponent;

        // ---------------- private helpers (DWARF :459-:727) ----------------
        // X360 0x82333238 (DWARF :464 -- GetChallengeFromID :459 is not in this TU's ledger).
        const BrnResource::ChallengeListEntry* GetChallengeFromID(CgsID lChallengeID) const;
        s32 GetChallengeIndex(CgsID lChallengeID) const;
        ChallengeCompletionData* GetChallengeCompletionData(BrnNetwork::NetworkPlayerID lPlayerID); // DWARF :469 (not in this TU's X360 ledger)

        // X360 0x82333878 / 0x823336E8 / 0x823166C0 (DWARF :475/:480/:486).
        bool CheckCurrentLocation(const BrnResource::ChallengeListEntryAction* lpAction,
                                  const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface);
        bool CheckCurrentCar(const BrnResource::ChallengeListEntry* lpChallenge,
                             const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface);
        bool CheckForModifiers(const BrnResource::ChallengeListEntryAction* lpAction,
                               const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface);

        // X360 0x8234E210 / 0x82351990 (DWARF :492/:499).
        void UpdateRemotePlayerSuccessStatus(const CgsModule::BaseEventQueue<GameStateModuleIO::CompletedFburnChallengesData>* lpCompletedChallengeStatusQueue,
                                             TGameActionQueue* lpActionQueue);
        void UpdateRemoteRequests(TGameActionQueue* lpActionQueue,
                                  const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                                  bool lbIsOnline);

        // X360 0x82353008 / 0x82345FD0 (DWARF :509/:515).
        void UpdateRunning(f32 lfTimeStep, s32 liFramesSinceNetworkStart, TGameActionQueue* lpActionQueue,
                           const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                           bool lbIsOnline, bool lbIsTimerFrequency50Hz);
        void UpdateResults(f32 lfTimeStep, TGameActionQueue* lpActionQueue);

        // X360 0x823460A0 / 0x82351AF8 (DWARF :526/:534).
        void UpdateActionSuccess(const BrnResource::ChallengeListEntryAction* lpAction, s32 liActionIndex,
                                 EChallengeStatus leChallengeStatus, EActiveRaceCarIndex leActiveRaceCarIndex,
                                 s32 liNumPlayers, TGameActionQueue* lpActionQueue, bool lbIsOnline);
        void UpdateArbitration(f32 lfTimeStep, TGameActionQueue* lpActionQueue,
                               const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                               bool lbIsOnline);
        // X360 0x8233AFF8 / 0x82350340 (DWARF :540/:546).
        void UpdateFreeburnSkillsThisFrame(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                                           f32 lfTimeStep);
        bool UpdateArbitrationSuccess(s32 liActionIndex, TGameActionQueue* lpActionQueue);

        // X360 0x82346918 / 0x823236D0 (DWARF :551/:557).
        void WriteDataToOutput(GameStateModuleIO::OutputBuffer* lpOutputBuffer);
        void WriteDataToOutputForTarget(GameStateModuleIO::FreeburnChallengeUpdateAction* lpUpdateAction, s32 liActionIndex);

        // X360 0x82347190 / 0x8233B410 (DWARF :566/:575).
        void UpdateChallenge(f32 lfTimeStep, s32 liFramesSinceNetworkStart,
                             const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                             TGameActionQueue* lpActionQueue, bool lbIsOnline);
        EChallengeStatus UpdateAction(s32 liActionIndex, const BrnResource::ChallengeListEntryAction* lpAction, f32 lfTimeStep,
                                      const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                                      bool lbIsOnline);

        // X360 0x82316968 / 0x82323FF8 (DWARF :584/:590).
        void UpdateCurrentActionScore(s32 liActionIndex, bool lbIsLocationOk, EFreeburnSkill leFreeburnSkill,
                                      EActiveRaceCarIndex leActiveRaceCarIndex, f32 lfScore);
        bool UpdateLocationOKStatusChange(bool lbWasLocationOk, bool lbIsLocationOk);

        // X360 0x82333BB8 / 0x823345B0 (DWARF :596/:601).
        void UpdateLeaptCars(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
                             f32 lfTimeStep);
        void UpdateBurnouts(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface);

        // DWARF :605/:609/:613 (not in this TU's X360 ledger).
        bool IsChallengeCountdownTimerDone();
        bool IsConvoyTimerDone();
        bool IsResultsTimerDone();

        // X360 0x82316DE8 / 0x82316F00 / 0x82317020 (DWARF :617/:622/:627).
        bool ReceivedSuccessUpdatesFromAllPlayers();
        s32  GetNumPlayerSucceeding(s32 liActionIndex);
        s32  GetNumPlayersContributing(s32 liActionIndex);

        // DWARF :631/:635/:639 (not in this TU's X360 ledger).
        void ResetChallengeCountdownTimer();
        void ResetConvoyTimer();
        void ResetChallengeResultsTimer();

        // X360 0x823232B0 / (UpdateConvoyTimer not in this TU's ledger) / 0x82323408
        // (DWARF :643/:647/:652).
        bool UpdateTimer(f32 lfTimeStep);
        bool UpdateConvoyTimer(f32 lfTimeStep);
        bool UpdateResultsTimer(f32 lfTimeStep);

        // X360 0x82333368 (DWARF :657).
        bool IsPointInTriggerRegion(const Vector3* lpPoint, CgsID lTriggerID);

        // X360 0x82323498 / 0x82323158 (DWARF :660/:663).
        void ClearFreeburnSkillsThisFrame();
        void ClearPlayerSuccessData();

        // X360 0x823167A0 / 0x8233B2A0 (DWARF :671/:680).
        // IsSkillScoreCurrentlySuccessful X360-vs-DWARF DRIFT: the PS3 DWARF declares FIVE params
        // (lfScore, lpAction, lbIsCumulativeArbitration, liTargetValueIndex, lbLargerScoresAreBetter);
        // the X360 build threads TWO extra middle params -- liActionIndex (r8) + leActiveRaceCarIndex
        // (r9) -- used by the action-type-6 (SEQUENCE) branch to probe
        // maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] (asm 8*r9 + 4*r8 + 0x4A8).
        // Register map at the callee 0x823167A0: f1=lfScore (r4 slot shadowed), r5=lpAction,
        // r6=lbIsCumulativeArbitration (UpdateAction's 38 callsites all pass their lbIsOnline here),
        // r7=liTargetValueIndex (GetTargetValue index), r8=liActionIndex, r9=leActiveRaceCarIndex,
        // r10=lbLargerScoresAreBetter (the top-level >=target vs <=target selector).
        bool IsSkillScoreCurrentlySuccessful(f32 lfScore, const BrnResource::ChallengeListEntryAction* lpAction,
                                             bool lbIsCumulativeArbitration, s32 liTargetValueIndex,
                                             s32 liActionIndex, EActiveRaceCarIndex leActiveRaceCarIndex,
                                             bool lbLargerScoresAreBetter);
        f32  GetCurrentSkillScore(s32 liActionIndex, bool lbIsLocationOk, EFreeburnSkill leFreeburnSkill,
                                  BrnResource::ChallengeListEntryAction::EChallengeCoopType leCoopType,
                                  bool lbIsOnline, f32 lfTimeStep);

        // X360 0x82324290 / 0x82317140 (DWARF :686/:692).
        void SetCurrentSkillScore(EFreeburnSkill leFreeburnSkill, f32 lfScore, bool lbBankImmediately);
        void BankSkillScore(EFreeburnSkill leFreeburnSkill, f32 lfScore);

        // X360 0x823244F8 / 0x82333100 / 0x82323DF8 (DWARF :697/:702/:707).
        void ResetCurrentChallengeData(s32 liActionIndex);
        void CheckForOnlineChallengeUnlocks();
        void SetRemotePlayersChallengeCompleted(s32 liChallengeIndex);

        // DWARF :710 (not in this TU's X360 ledger).
        void RenderHUD();
        // X360 0x8233B070 (DWARF :713).
        f32  GetScaleFactor();

        // DWARF :724/:727. HackAllChallenges is not in this TU's X360 ledger;
        // UnHackAllChallenges is X360 0x82333040 (restores the numPlayers nibble backed up in
        // the high nibble of ChallengeListEntry byte +0xD3).
        void HackAllChallenges(s32 liNumPlayers);
        void UnHackAllChallenges();

        // X360 0x823246F0 (class-bucket ledger TU; called by ProcessEvent/UpdateChallenge).
        // Bodied in BrnChallengeManager.cpp. Its EChallengeActionType->skill dispatch uses
        // KAI_CHALLENGE_ACTION_TYPE_TO_FREEBURN_SKILL (dword_82021288, dump-verified).
        void ResetActionData(s32 liActionToResetIndex);

        // X360 0x8233E530 (class-bucket helper the committed debug component calls).
        s32 CountCompletedChallenges();

        // X360 0x82334740 (class-bucket helper PostWorldUpdate calls; lpStuntScoreInfo is the
        // un-homed external stunt-score snapshot, fields read by attested byte offset).
        void UpdateStuntScores(const void* lpStuntScoreInfo);

        static void _AssertLayout();   // never called; pins the X360-attested offsets below

        // ====================================================================
        // DATA -- DWARF order (BrnChallengeManager.h:381-453, :719-720), X360 offsets annotated.
        // Pointer-free through +0x5C0: those offsets are absolute on the x64 gate too.
        // ====================================================================
        CgsContainers::ObjectPool<StoredLeapingData, 7, s32> mPotentiallyLeaptCars; // +0x000 (:381; pool 0x00-0xE0, free queue 0xE0, numFree 0xFC, occupancy u64 0x100, 16-align pad -> 0x110)
        f32   mfLeapCarsValidTimer;                                  // +0x110 (:382; -1.0f == invalid)
        s32   miNumCarsLeapt;                                        // +0x114 (:383)
        bool  mbUpdateLeaptCars;                                     // +0x118 (:384) (+7 pad)
        Array<CgsID, 80u> maBillboardsCollected;                     // +0x120 (:386; ids 0x120-0x3A0, miCount @0x3A0, +4 pad; KI_MAX_BILLBOARDS_TO_TRACK == 80)
        CgsContainers::FastBitArray<120> maPlayerSuccessUpdateArray[KI_MAX_CHALLENGE_PLAYERS];       // +0x3A8 (:387; 16B each)
        GameStateModuleIO::EFreeburnChallengeSuccess maaePlayersSuccessStatus[KI_MAX_CHALLENGE_PLAYERS][KI_MAX_CHALLENGE_ACTIONS]; // +0x428 (:388)
        f32   maafCurrentActionsScores[KI_MAX_CHALLENGE_PLAYERS][KI_MAX_CHALLENGE_ACTIONS];          // +0x468 (:389)
        f32   maafCumulativeContributions[KI_MAX_CHALLENGE_PLAYERS][KI_MAX_CHALLENGE_ACTIONS];       // +0x4A8 (:390)
        f32   mafBankedActionScores[KI_FREEBURN_SKILL_COUNT_X360];   // +0x4E8 (:391; PS3 [19] -> X360 [38])
        f32   mafCumulativeActionScores[KI_MAX_CHALLENGE_ACTIONS];   // +0x580 (:392)
        s32   maiRemainingTarget[KI_MAX_CHALLENGE_ACTIONS];          // +0x588 (:393)
        bool  mabReceivedSuccessUpdates[KI_MAX_CHALLENGE_PLAYERS];   // +0x590 (:394)
        bool  mabPlayerStartedChallenge[KI_MAX_CHALLENGE_PLAYERS];   // +0x598 (:395)
        s32   maiCrashedWithChallengePlayer[KI_MAX_CHALLENGE_PLAYERS]; // +0x5A0 (:396)
        const BrnResource::VehicleList* mpVehicleList;               // +0x5C0 (:397) -- FIRST POINTER; x64 offsets drift from here (+4 pad on X360)
        ChallengeCompletionData maChallengeCompletionData[KI_MAX_REMOTE_PLAYERS]; // +0x5C8 (:399; stride 0x108)
        CgsContainers::FastBitArray<2000> mLocalChallengeCompletionData; // +0xD00 (:400; DWARF type CompletedFburnChallengesData::CompletedFburnChallenges == this layout)
        GameStateModuleIO::LastSecondChallengeSuccess mLastSecondSuccessStatus; // +0xE00 (:402; DWARF type FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess == FastBitArray<60>)
        EChallengeManagerStatus meChallengeManagerStatus;            // +0xE08 (:405)
        const BrnResource::ChallengeListEntry* mpCurrentChallenge;   // +0xE0C (:406)
        s32   miCurrentChallengeAction;                              // +0xE10 (:407)
        s32   miCurrentArbitrationIndex;                             // +0xE14 (:408)
        f32   mfChallengeTimer;                                      // +0xE18 (:409)
        bool  mbChallengeTimerRunning;                               // +0xE1C (:410)
        f32   mfConvoyTimer;                                         // +0xE20 (:411)
        bool  mbConvoyTimerRunning;                                  // +0xE24 (:412)
        f32   mfResultsTimer;                                        // +0xE28 (:413)
        bool  mbResultsTimerRunning;                                 // +0xE2C (:414)
        EChallengeStatus meLocalChallengeStatus;                     // +0xE30 (:415)
        bool  mbChallengeRequiresLeapCars;                           // +0xE34 (:416)
        bool  mbRemoteStartPending;                                  // +0xE35 (:419)
        bool  mbRemoteTriggerPending;                                // +0xE36 (:420)
        bool  mbRemoteEndPending;                                    // +0xE37 (:421)
        const BrnResource::ChallengeList*  mpFreeburnChallengeList;  // +0xE38 (:423)
        const RoadRulesManager*            mpRoadRulesManager;       // +0xE3C (:424)
        const TriggerQueryManager*         mpTriggerQueryManager;    // +0xE40 (:425)
        BrnProgression::ProgressionManager* mpProgression;           // +0xE44 (:427)
        GameStateModule*                   mpGameStateModule;        // +0xE48 (:428)
        ModeManager*                       mpModeManager;            // +0xE4C (X360-only drift member; Construct arg 2, "lpModeManager")
        bool  mabBankedSkillThisFrame[KI_FREEBURN_SKILL_COUNT_X360]; // +0xE50 (:431)
        bool  mabActiveSkillThisFrame[KI_FREEBURN_SKILL_COUNT_X360]; // +0xE76 (:432)
        f32   mafActiveSkillValue[KI_FREEBURN_SKILL_COUNT_X360];     // +0xE9C (:433)
        f32   mafCachedActiveSkillValueOnLocationEnter[KI_FREEBURN_SKILL_COUNT_X360]; // +0xF34 (:434) (+4 pad @0xFCC)
        CgsContainers::FastBitArray<38> mScoresSetThisFrameBitArray; // +0xFD0 (:435; PS3 <19> -> X360 <38>, range assert prints "max bits: 38")
        bool  mabIsLocationOk[KI_MAX_CHALLENGE_ACTIONS];             // +0xFD8 (:438)
        bool  mabIndividualActionsSuccessUpdateSent[KI_MAX_CHALLENGE_ACTIONS]; // +0xFDA (:441)
        s32   maChallengePlayerCounts[KI_MAX_CHALLENGE_PLAYERS];     // +0xFDC (:444; [numPlayers-1] histogram of the challenge list)
        s32   miFramesSinceNetworkStart;                             // +0xFFC (:447; PreWorldUpdate caches its arg here every frame)
        s32   miLastChallengeResetFrame;                             // +0x1000 (X360-only; -1 init; challenge-reset events latch it, stale success events are dropped against it)
        ChallengeManagerDebugComponent mChallengeManagerDebugComponent; // +0x1004 (:453; vtbl+mbActive+mpNext base, back-ptr @+0x1010, index @+0x1014, pending @+0x1018)
        bool  mbWereAllChallengesOnePlayer;                          // +0x101C (:719)
        bool  mbWereAllChallengesTwoPlayer;                          // +0x101D (:720) (object pads to 0x1020)

        // Static (file-scope on X360: byte_82FAD9C0/C1) "make all challenges N-player" debug
        // toggles (DWARF `extern bool`, :717/:718).
        static bool mbChallengesAreAllOnePlayer;
        static bool mbChallengesAreAllTwoPlayer;
    };
}
