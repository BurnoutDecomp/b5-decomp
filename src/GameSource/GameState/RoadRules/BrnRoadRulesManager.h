// BrnGameState::RoadRulesManager -- the per-road Time / Showtime ("crash") road rules: which rule
// is active, the rule attempt currently running on a road, its clocks and score, and the
// enter-road / leave-road / start-rule / end-rule game actions it posts.
//
// The class is laid out in full. Member names and order come from the debug information; every
// console offset in the `// +0xNN` comments is read off the console code of this class's own
// methods (Construct stores almost every member). The embedded debug component makes every
// member after it sit at a different host offset; _AssertLayout() below states what holds on
// the host.
#ifndef BRN_ROAD_RULES_MANAGER_H
#define BRN_ROAD_RULES_MANAGER_H

#include "types.hpp"
#include "GameSource/GameState/BrnGameStateSharedIO.h"          // GameStateModuleIO::GameActionQueue, EGameModeType
#include "GameSource/GameState/BrnGameStateTypes.h"             // BrnGameState::EActiveRoadRule
#include "GameSource/GameState/RoadRules/BrnRoadRulesDebugComponent.h"  // mRoadRulesDebugComponent (by value)
#include "BrnCommonTypes.h"                                     // CgsID (u64)
#include "SharedClasses/StreetData/BrnStreetData.h"             // BrnStreetData::RoadIndex, KI_INVALID_ROAD_INDEX, ScoreType
#include "SharedClasses/DataLists/ChallengeListEntry.h"         // BrnResource::ChallengeListEntry::EFreeburnChallengeStyle

#include <cstddef>   // offsetof (_AssertLayout)

namespace BrnGameState
{
    // Pointer members / parameters only. Real homes:
    //   StreetManager   -> GameSource/GameState/StreetData/BrnGameStateStreetManager.h (struct)
    //   ModeManager     -> GameSource/GameState/ModeManager/BrnModeManager.h
    //   TrainingManager -> GameSource/GameState/TrainingManager/BrnTrainingManager.h
    struct StreetManager;
    class ModeManager;
    class TrainingManager;

    // Home: GameSource/GameState/BrnGameStateModuleIO.h.
    namespace GameStateModuleIO
    {
        struct OutputBuffer;
        struct ControllerInput;
    }

    class RoadRulesManager
    {
        friend class RoadRulesDebugComponent;

    public:
        // Returned by GetCurrentRoadID when there is no current road (the console returns 0).
        static const CgsID K_INVALID_ID;

        // Called by GameStateModule::Construct with (&mStreetManager, &mModeManager,
        // &mTrainingManager). Constructs the embedded debug component (inlined into this body)
        // and initialises every member except mfStuntRuleComboTimeout and miCrashScore.
        void Construct(StreetManager* lpStreetManager, ModeManager* lpModeManager,
                       TrainingManager* lpTrainingManager);

        // Called once per frame by GameStateModule::UpdateRoadRulesManager. Twelve arguments: the
        // two f32 arrive in floating-point registers (each still owns a skipped integer slot), and
        // leGameModeType, lbCarSelectActive, lbDisableSwitchingOnline and leFreeburnChallengeStyle
        // are passed on the stack. lbInAir and lbPlayerIsCrashing are passed but never read.
        void Update(const GameStateModuleIO::ControllerInput*               lpControllerInput,
                    BrnStreetData::RoadIndex                                 liCurrentRoadIndex,
                    f32                                                      lfTimeStep,
                    bool                                                     lbInAir,
                    bool                                                     lbPlayerIsCrashing,
                    GameStateModuleIO::OutputBuffer*                         lpOutputBuffer,
                    bool                                                     lbShowtimeActive,
                    GameStateModuleIO::EGameModeType                         leGameModeType,
                    f32                                                      lfPlayerNoInputTime,
                    bool                                                     lbCarSelectActive,
                    bool                                                     lbDisableSwitchingOnline,
                    BrnResource::ChallengeListEntry::EFreeburnChallengeStyle leFreeburnChallengeStyle);

        // GameStateModule::ProcessGameEvents: the GUI asked for a road's rule data. lRoadId == 0
        // re-sends the current road; otherwise the matching road index is looked up.
        void OnRoadRulesDataRequest(CgsID lRoadId, GameStateModuleIO::GameActionQueue* lpGameActionQueue);

        // TriggerQueryManager::UpdateTriggers: does any road carry this road-limit id?
        // lRoadLimitId is never read (it only feeds the caller's assert message).
        bool IsRoadLimitRegionValid(CgsID lRoadLimitId, CgsID lRoadLimitGroupId) const;

        // TriggerQueryManager::ProcessPlayerTriggers: the player crossed a road-limit region.
        // lRoadLimitId is the region's group id (else its id), sign-extended to 64 bits by the
        // caller; lbEntryDirection is dot(velocity, region direction) > 0; the last byte is the
        // player car's crashing flag.
        void OnRoadLimit(CgsID lRoadLimitId, bool lbEntryDirection,
                         GameStateModuleIO::OutputBuffer* lpOutputBuffer, bool lbIsPlayerCarCrashing);

        // GameStateModule::OnProfileLoaded: end the time rule and the crash rule if either runs.
        void QuitAnyActiveRules(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

        // Inlined into GameStateModule::UpdateRoadRulesManager (a single store to +0x60).
        void SetShowtimeScore(s32 liScore) { miCrashScore = liScore; }

        // Inlined into its readers: a rule of that score type is running on a road.
        bool IsTimeRuleActive() const  { return IsValidRoad(maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME]); }
        bool IsCrashRuleActive() const { return IsValidRoad(maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_CRASH]); }

        // Inlined into GameStateModule::ProcessGameEvents (a single byte store to +0x70).
        void SetSwitchingActive(bool lbSwitchingActive) { mbSwitchingActive = lbSwitchingActive; }

        // GameStateModule::ProcessGameEvents: the GUI picked a rule. Tail-calls SendActiveRuleState.
        void SetActiveRoadRule(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                               EActiveRoadRule leActiveRoadRule);

        CgsID GetCurrentRoadID() const;

        // GameStateModule::OnEnterOnline (true) and ProcessGameEvents (false / event byte):
        // switch between the offline and online flavour of the active rule.
        void SetRoadRulesMode(GameStateModuleIO::OutputBuffer* lpOutputBuffer, bool lbMode);

    private:
        // Returns mbRoadRulesNotAllowed's new value (the rules are suspended this frame).
        bool UpdateSwitchingRoadRulesOnOrOff(GameStateModuleIO::GameActionQueue*                 lpGameActionQueue,
                                             GameStateModuleIO::OutputBuffer*                    lpOutputBuffer,
                                             GameStateModuleIO::EGameModeType                    leGameModeType,
                                             bool                                                lbCarSelectActive,
                                             BrnResource::ChallengeListEntry::EFreeburnChallengeStyle leFreeburnChallengeStyle,
                                             f32                                                 lfTimeStep,
                                             bool                                                lbFreeburnActive);

        void UpdateActiveRoadRule(GameStateModuleIO::OutputBuffer*                    lpOutputBuffer,
                                  GameStateModuleIO::GameActionQueue*                 lpGameActionQueue,
                                  bool                                                lbStartEventPressed,
                                  bool                                                lbDisableSwitchingOnline,
                                  BrnResource::ChallengeListEntry::EFreeburnChallengeStyle leFreeburnChallengeStyle);

        // Posts action 282 (the active rule) and action 286 (online or not), mirrors the rule's
        // score type into StreetManager::meActiveRoadRuleType, latches mbIsOnlineMode.
        void SendActiveRuleState(GameStateModuleIO::GameActionQueue* lpGameActionQueue);

        void UpdateTimeRule(GameStateModuleIO::OutputBuffer* lpOutputBuffer, f32 lfTimeStep);

        void OnShowtimeStart(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

        // Action 273 / action 274. Both are no-ops for an invalid road index.
        void OnEnterRoad(GameStateModuleIO::GameActionQueue* lpGameActionQueue, BrnStreetData::RoadIndex liRoadIndex);
        void OnLeaveRoad(GameStateModuleIO::GameActionQueue* lpGameActionQueue, BrnStreetData::RoadIndex liRoadIndex);

        // Action 277.
        void OnStartRule(GameStateModuleIO::GameActionQueue* lpGameActionQueue, BrnStreetData::RoadIndex liRoadIndex,
                         BrnStreetData::ScoreType leScoreType, bool lbAllowTimeout);

        // Action 278; scores the attempt through OnScoreCompleted when lbAllowScoring.
        void OnEndRule(GameStateModuleIO::OutputBuffer* lpOutputBuffer, BrnStreetData::ScoreType leScoreType,
                       bool lbAllowScoring);

        // Action 279.
        void OnUpdateActiveRoadScores(GameStateModuleIO::GameActionQueue* lpGameActionQueue);

        void OnScoreCompleted(GameStateModuleIO::OutputBuffer* lpOutputBuffer, BrnStreetData::ScoreType leScoreType);

        // Action 286. Has no console body of its own: inlined into SendActiveRuleState.
        void SendRoadRuleModeSwitchMessage(GameStateModuleIO::GameActionQueue* lpGameActionQueue, bool lbIsOnline);

        // Inlined everywhere: compare against the KI_INVALID_ROAD_INDEX constant (-1).
        bool IsValidRoad(s32 liRoadIndex) const { return liRoadIndex != BrnStreetData::KI_INVALID_ROAD_INDEX; }

        // Inlined everywhere, in exactly these compare orders.
        bool IsRoadRuleCrash(EActiveRoadRule leActiveRoadRule)
        {
            return leActiveRoadRule == E_ACTIVE_ROAD_RULE_ONLINE_CRASH || leActiveRoadRule == E_ACTIVE_ROAD_RULE_OFFLINE_CRASH;
        }
        bool IsRoadRuleTime(EActiveRoadRule leActiveRoadRule)
        {
            return leActiveRoadRule == E_ACTIVE_ROAD_RULE_ONLINE_TIME || leActiveRoadRule == E_ACTIVE_ROAD_RULE_OFFLINE_TIME;
        }
        bool IsRoadRuleOnline(EActiveRoadRule leActiveRoadRule)
        {
            return leActiveRoadRule == E_ACTIVE_ROAD_RULE_ONLINE_CRASH || leActiveRoadRule == E_ACTIVE_ROAD_RULE_ONLINE_TIME;
        }

        RoadRulesDebugComponent  mRoadRulesDebugComponent;        // +0x00 (console size 0x14)
        StreetManager*           mpStreetManager;                 // +0x14
        ModeManager*             mpModeManager;                   // +0x18
        TrainingManager*         mpTrainingManager;               // +0x1C
        BrnStreetData::RoadIndex miLastRoadIndex;                 // +0x20  the road the player is on (-1 none)
        BrnStreetData::RoadIndex maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_COUNT]; // +0x24 per ScoreType, -1 = no rule running
        CgsID                    mLastLimitId;                    // +0x30  the road-limit id that started the time rule
        EActiveRoadRule          meActiveRoadRule;                // +0x38
        EActiveRoadRule          mePreviousActiveRoadRule;        // +0x3C  (only Construct touches it)
        bool                     mbRoadRulesNotAllowed;           // +0x40
        f32                      mfTime;                          // +0x44  time-rule clock, seconds
        f32                      mfTimeScoreTimeout;              // +0x48
        f32                      mfTimeTarget;                    // +0x4C
        f32                      mfNextWarningTime;               // +0x50
        s32                      miNumWarningsDone;               // +0x54
        f32                      mfStuntTime;                     // +0x58
        f32                      mfStuntRuleComboTimeout;         // +0x5C  (no reader or writer in the class's code)
        s32                      miCrashScore;                    // +0x60  showtime score
        f32                      mfInRoadTimeout;                 // +0x64
        bool                     mbAllowExitRoadRulesAfterTimeout;// +0x68
        f32                      mfExitRoadRulesTime;             // +0x6C
        bool                     mbSwitchingActive;               // +0x70
        bool                     mbIsOnlineMode;                  // +0x71
        // console sizeof == 0x78 (CgsID alignment)

    public:
        // Never called; pins what holds on the host.
        //  - the debug component is the first member and the three pointers follow it directly;
        //  - from miLastRoadIndex to the end the class holds no pointer, and miLastRoadIndex is
        //    8-aligned on both builds, so every member of that run sits at the console's distance
        //    from miLastRoadIndex (console +0x20) and the run is 0x58 bytes long on both.
        static void _AssertLayout()
        {
            typedef RoadRulesManager R;
            static_assert(offsetof(R, mRoadRulesDebugComponent) == 0, "debug component is the first member");
            static_assert(offsetof(R, mpStreetManager)   == sizeof(RoadRulesDebugComponent), "console +0x14");
            static_assert(offsetof(R, mpModeManager)     == offsetof(R, mpStreetManager) + 1 * sizeof(void*), "console +0x18");
            static_assert(offsetof(R, mpTrainingManager) == offsetof(R, mpStreetManager) + 2 * sizeof(void*), "console +0x1C");
            static_assert(offsetof(R, miLastRoadIndex)   == offsetof(R, mpStreetManager) + 3 * sizeof(void*), "console +0x20");
            static_assert(offsetof(R, miLastRoadIndex) % 8 == 0, "tail run starts 8-aligned, as on the console");

            #define RRM_TAIL(member, consoleOffset) \
                static_assert(offsetof(R, member) - offsetof(R, miLastRoadIndex) == (consoleOffset) - 0x20, #member)
            RRM_TAIL(maiChallengeRoadIndex,            0x24);
            RRM_TAIL(mLastLimitId,                     0x30);
            RRM_TAIL(meActiveRoadRule,                 0x38);
            RRM_TAIL(mePreviousActiveRoadRule,         0x3C);
            RRM_TAIL(mbRoadRulesNotAllowed,            0x40);
            RRM_TAIL(mfTime,                           0x44);
            RRM_TAIL(mfTimeScoreTimeout,               0x48);
            RRM_TAIL(mfTimeTarget,                     0x4C);
            RRM_TAIL(mfNextWarningTime,                0x50);
            RRM_TAIL(miNumWarningsDone,                0x54);
            RRM_TAIL(mfStuntTime,                      0x58);
            RRM_TAIL(mfStuntRuleComboTimeout,          0x5C);
            RRM_TAIL(miCrashScore,                     0x60);
            RRM_TAIL(mfInRoadTimeout,                  0x64);
            RRM_TAIL(mbAllowExitRoadRulesAfterTimeout, 0x68);
            RRM_TAIL(mfExitRoadRulesTime,              0x6C);
            RRM_TAIL(mbSwitchingActive,                0x70);
            RRM_TAIL(mbIsOnlineMode,                   0x71);
            #undef RRM_TAIL
            static_assert(sizeof(R) - offsetof(R, miLastRoadIndex) == 0x78 - 0x20, "tail run length");
            static_assert(sizeof(EActiveRoadRule) == 4, "EActiveRoadRule is a 4-byte field");
        }
    };
}

#endif // BRN_ROAD_RULES_MANAGER_H
