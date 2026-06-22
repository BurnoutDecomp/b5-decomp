#pragma once

#include "types.hpp"

#include "GameSource/BurnoutConstants.h"                                          // E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
// NOTE: the project carries two same-valued EActiveRaceCarIndex enums -- the global one
// (BurnoutConstants.h, used by the vehicle/traffic IO headers) and BrnGameState::EActiveRaceCarIndex
// (BrnTakedownManagerTypes.h, used by ScoringSystem / CarData). This TU talks to the
// ScoringSystem/CarData API, so its member/method enum is the BrnGameState one; include its home
// here so the header declarations resolve to the SAME type the .cpp bodies see. Calls into the
// global-enum vehicle interface static_cast across the dup boundary in the .cpp.
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"          // BrnGameState::EActiveRaceCarIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                        // BrnNetwork::NetworkPlayerID (s32), BrnNetwork::Road::ChallengeIndex
#include "SharedClasses/StreetData/BrnChallengeData.h"                            // BrnStreetData::{ChallengeData, ChallengePlayerScoreEntry, ScoreType}
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"           // BrnStreetData::ChallengeHighScoreEntry
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h"        // BrnGameState::BurnoutSkillzData
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"                    // CgsContainers::FastBitArray<8> (mabDirtyFlags)

// ---------------------------------------------------------------------------
// BrnGameState::BurnoutSkillzManager  (DWARF home BrnBurnoutSkillzManager.h:53)
//
// The "burnout skillz" game-mode manager: tallies the per-player skill records (air
// time, drift, spin, near misses, ...), buffers and dispatches new road-rule scores,
// and reconciles online road-rule personal-bests against the local scoring system.
// It is embedded by value inside the online free-burn lobby mode (OnlineFreeBurnLobbyMode
// owns one and forwards Construct/PreWorldUpdate/ProcessNewRoadScore to it).
//
// Member run + types are DWARF-authoritative (BrnBurnoutSkillzManager.h:130-149), in
// declared order, accessed BY NAME (no raw-offset casts). The two 8-element int arrays
// occupy +0x00 and +0x20; mBufferedChallengeScore (ChallengePlayerScoreEntry) follows at
// +0x40; the four manager back-pointers, the per-frame scalars, the dirty-flags
// FastBitArray<8> (+0x90) and the crashed-state bool (+0x98) close out the layout. The
// X360 asm pins those offsets (e.g. a1[28]==mpScoringSystem@+0x70, a1[35]==miCurrentRoadIndex
// @+0x8C, a1[36]==mabDirtyFlags@+0x90), and the named members here land on exactly those.
//
// The heavy IO/queue/interface dependency types (the input/output buffers, the per-car
// race-car output interface, the network road-rules queue, the manager peers) are only
// touched through pointers in this TU's bodies, so they are forward-declared here and the
// .cpp pulls their real homes.
// ---------------------------------------------------------------------------

// ---- forward declarations (passed by pointer only; real homes pulled by the .cpp) ----
namespace CgsModule { template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue; }

namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }
namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }

namespace BrnGameState
{
class ScoringSystem;
class MugshotManager;
class ModeManager;
class StreetManager;

namespace GameStateModuleIO
{
    struct PreWorldInputBuffer;
    struct PostWorldInputBuffer;
    struct OutputBuffer;
    class  GameEventQueue;                 // == CgsModule::VariableEventQueue<1536,16>
    class  GameActionQueue;                // == CgsModule::VariableEventQueue<13312,16>
    struct NetworkToGameStateInterface;
    struct OnlineRoadRulesPersonalBestRecvEvent;
}
}

namespace BrnGameState
{
// The achievement manager the manager caches (StuntModeScoring::AchievementManager is a typedef of
// this; pointer only here).
class AchievementManagerPS3;
}

namespace BrnGameState
{
class BurnoutSkillzManager
{
public:
    // BrnBurnoutSkillzManager.h:59 / X360 0x82332688
    void Construct(ModeManager* lpModeManager);

    // BrnBurnoutSkillzManager.h:67 / X360 0x8234D410
    void PreWorldUpdate(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                        GameStateModuleIO::OutputBuffer* lpOutput,
                        bool lbExitingFreeburnLobby);

    // BrnBurnoutSkillzManager.h:72 (body in another TU -- not this 16-function TU)
    void UpdatePostWorld(const GameStateModuleIO::PostWorldInputBuffer* lpInput);

    // BrnBurnoutSkillzManager.h:78 / X360 0x82322988
    void SendUpdatePlayerSkillsEvent(EActiveRaceCarIndex leActiveRaceCarIndex, bool lbShowHudMessage);

    // BrnBurnoutSkillzManager.h:87 / X360 0x82345348
    void ProcessNewRoadScore(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                             BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                             BrnStreetData::ScoreType leScoreType,
                             BrnNetwork::Road::ChallengeIndex liChallengeIndex,
                             EActiveRaceCarIndex leLocalActiveRaceCarIndex);

    // BrnBurnoutSkillzManager.h:94 (body in another TU)
    void BufferNewRoadScore(BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                            BrnStreetData::ScoreType leScoreType,
                            BrnNetwork::Road::ChallengeIndex liChallengeIndex);

    // BrnBurnoutSkillzManager.h:99 / X360 0x82322B98
    void OnEnterRoad(BrnNetwork::Road::ChallengeIndex liRoadIndex);

    // BrnBurnoutSkillzManager.h:104 (trivial setter; body in another TU)
    void SetStreetManager(StreetManager* lpStreetManager);

    // BrnBurnoutSkillzManager.h:108 (trivial setter; body in another TU)
    void SetMugshotManager(MugshotManager* lpMugshotManager);

    // BrnBurnoutSkillzManager.h:113 (body in another TU)
    void OnModeEnd(bool lbExitingFreeburnLobby);

private:
    // BrnBurnoutSkillzManager.h:153 / X360 0x82322480
    void ClearAllBurnoutSkillzData();

    // BrnBurnoutSkillzManager.h:157 (body in another TU)
    void ClearAllCurrentSkillzEarningData();

    // BrnBurnoutSkillzManager.h:164 / X360 0x823327B8
    void UpdateBoostChains(BurnoutSkillzData* lpSkillzData,
                           const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                           EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex);

    // BrnBurnoutSkillzManager.h:169 / X360 0x8233A668
    void UpdateLobbyRoadRulesScores(const GameStateModuleIO::NetworkToGameStateInterface* lpRoadRulesRecvQueue);

    // BrnBurnoutSkillzManager.h:176 / X360 0x82345740
    void ProcessNetworkRoadRulePB(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                  const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent* lpPersonalBestEvent,
                                  EActiveRaceCarIndex leLocalPlayersActiveRaceCarIndex);

    // BrnBurnoutSkillzManager.h:183 / X360 0x823227C0
    bool GetRoadRuleHighScore(BrnNetwork::NetworkPlayerID lNetworkPlayerID,
                              BrnNetwork::Road::ChallengeIndex liIndex,
                              BrnStreetData::ChallengeHighScoreEntry* lpData);

    // BrnBurnoutSkillzManager.h:190 / X360 0x823228A0
    void SetRoadRuleHighScore(BrnNetwork::NetworkPlayerID lNetworkPlayerID,
                              BrnNetwork::Road::ChallengeIndex liIndex,
                              BrnStreetData::ChallengeHighScoreEntry* lpData);

    // BrnBurnoutSkillzManager.h:197 / X360 0x82345AC0
    void ProcessGameEventInputQueuePreWorld(const GameStateModuleIO::GameEventQueue* lpEventQueue,
                                            GameStateModuleIO::GameActionQueue* lpActionQueue,
                                            EActiveRaceCarIndex leLocalPlayersActiveRaceCarIndex);

    // BrnBurnoutSkillzManager.h:205 / X360 0x823328D0
    void ProcessGameEventInputQueuePostWorld(BurnoutSkillzData* lpSkillzData,
                                             const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                             const GameStateModuleIO::GameEventQueue* lpQueue,
                                             EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex);

    // BrnBurnoutSkillzManager.h:214 / X360 0x823225C0
    void SetNewSkillIfGreater(BurnoutSkillzData::EBurnoutSkillType leSkillType,
                              BurnoutSkillzData* lpSkillzData,
                              EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex,
                              f32 lfNewValue);

    // BrnBurnoutSkillzManager.h:219 (body in another TU)
    bool AreAllCarWheelsOnTheGround(const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState);

    // BrnBurnoutSkillzManager.h:224 / X360 0x82322B18
    EActiveRaceCarIndex GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID lNetworkPlayerID);

    // BrnBurnoutSkillzManager.h:228 / X360 0x82322C30
    void UpdateBurnoutSkillzTotals(EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex);

    // ===== data members (DWARF declared order + types; X360-pinned offsets) =====
    // BrnBurnoutSkillzManager.h:130 -- per-active-car "is this player on a road" int flag (@ +0x00).
    s32 maiRoadsPlayersAreOn[E_ACTIVE_RACE_CAR_INDEX_COUNT];

    // BrnBurnoutSkillzManager.h:131 -- per-active-car "skills updated" bit flags (@ +0x20). The
    // event/score paths OR bit 4 (new-record-this-frame), bit 8 (show-HUD), 0xE (all) into a slot.
    s32 maiBurnoutSkillsUpdatedFlags[E_ACTIVE_RACE_CAR_INDEX_COUNT];

    // BrnBurnoutSkillzManager.h:133 -- the road score buffered between PreWorld passes (@ +0x40).
    BrnStreetData::ChallengePlayerScoreEntry mBufferedChallengeScore;
    // BrnBurnoutSkillzManager.h:134 -- challenge index of the buffered score (@ +0x68).
    BrnNetwork::Road::ChallengeIndex         mBufferedScoreChallengeIndex;
    // BrnBurnoutSkillzManager.h:135 -- score type of the buffered score (@ +0x6C; sentinel == E_SCORE_TYPE_COUNT).
    BrnStreetData::ScoreType                 meBufferedScoreType;

    // BrnBurnoutSkillzManager.h:137-140 -- manager back-pointers (@ +0x70/+0x74/+0x78/+0x7C).
    ScoringSystem*                       mpScoringSystem;
    StreetManager*                       mpStreetManager;
    MugshotManager*                      mpMugshotManager;
    AchievementManagerPS3*               mpAchievementManager;  // == StuntModeScoring::AchievementManager*

    // BrnBurnoutSkillzManager.h:142-145 -- per-frame skill-earning scalars (@ +0x80..+0x8C).
    f32                              mfCurrentTimeInAir;
    s32                              miCurrentBoostChains;
    s32                              miCurrentTrafficChain;
    BrnNetwork::Road::ChallengeIndex miCurrentRoadIndex;

    // BrnBurnoutSkillzManager.h:147 -- per-active-car "skills dirty -> send update" bit set (@ +0x90).
    CgsContainers::FastBitArray<E_ACTIVE_RACE_CAR_INDEX_COUNT> mabDirtyFlags;

    // BrnBurnoutSkillzManager.h:149 -- "player is mid-crash" latch (@ +0x98).
    bool mbIsInCrashedState;
};
}
