// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/GameModes/BrnBurnoutSkillzManager.cpp
//
// BrnGameState::BurnoutSkillzManager -- the burnout-skillz game-mode manager.
// Semantic reconstruction (X360 asm authoritative) of all 16 functions owned by this
// TU: Construct / PreWorldUpdate / SendUpdatePlayerSkillsEvent / ProcessNewRoadScore /
// OnEnterRoad / ClearAllBurnoutSkillzData / UpdateBoostChains / UpdateLobbyRoadRulesScores /
// ProcessNetworkRoadRulePB / Get/SetRoadRuleHighScore / ProcessGameEventInputQueue{Pre,Post}World /
// SetNewSkillIfGreater / GetActiveRaceCarIndex / UpdateBurnoutSkillzTotals.
//
// Members are accessed BY NAME. The dirty-flags FastBitArray<8> bit walks that the X360
// inlines (SetBit / SetAll / GetFirstBitSet / GetNextBitSet, each wrapped in CgsDev::StrStream
// out-of-range assert machinery) are reconstructed as clean FastBitArray<8> API calls; that
// folded assert/StrStream scaffolding is intentionally NOT reproduced bit-for-bit (assert
// parity is benign). The per-car maiBurnoutSkillsUpdatedFlags[] bit-OR stores (bits 4/8/0xE)
// are kept as the named-member integer ORs the asm performs.
//
// House CGS_ASSERT replaces the inlined CgsDev::Assert::Begin/Fire/EndAssert triples.
// ============================================================================

#include "GameSource/GameState/ModeManager/GameModes/BrnBurnoutSkillzManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"            // ScoringSystem + CarData
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                      // ModeManager::GetScoringSystem
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"                // MugshotManager::ProcessBeatenRoadRuleEvent
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"            // StreetManager::GetStreetData
#include "SharedClasses/StreetData/BrnStreetData.h"                               // BrnStreetData::StreetData / Road (GetRoad/GetId)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnFreeburnSkillzTotalChange
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"         // StuntModeScoring::AchievementManager (== AchievementManagerPS3)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                            // PreWorld/PostWorld/OutputBuffer, GameEventQueue, GameActionQueue
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface, BoostOutputInfo, RaceCarState
#include "GameSource/Network/Utilities/BrnNetworkRounder.h"                       // BrnNetwork::NetworkRounder::RoundFloatToAccuracy
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue<>
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                       // BrnNetwork::PlayerName, RoadRulesRecvData, RoadRulesMessageData

#include <cstring>  // memcpy

namespace BrnGameState
{

// The race-car output interface, boost-info and race-car-state types this TU reads through.
typedef BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo BoostOutputInfo;
typedef BrnPhysics::Vehicle::RaceCarState                RaceCarState;

// The two GameStateModuleIO event queues are the concrete VariableEventQueue
// instantiations the X360 dispatches to (GameEventQueue == <1536,16>,
// GameActionQueue == <13312,16>); alias them so the bodies read the events / push the
// actions through their real templated API by name.
typedef CgsModule::VariableEventQueue<1536, 16>  GameEventQueueImpl;
typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;

// KI_MAX_CHALLENGES (the road-rule challenge table width, X360 0x40) now comes from
// the frozen BrnGameStateStreetManager.h (its DWARF home, BrnGameStateStreetManager.h:102);
// the former file-local duplicate was removed with the wave-B StreetManager freeze.

// ----------------------------------------------------------------------------
// Event payload views.
//
// These per-event-type structs are the byte layouts the X360 reads off each
// VariableEventQueue record payload (no DWARF/leak home exists for them in this slice).
// Fields are named by the X360 access (offset -> name); each event is read back through
// a typed pointer view of the queued payload so the bodies stay member-by-name.
// ----------------------------------------------------------------------------
struct InAirEventView                 { f32 mfTimeInAir; };                 // case 'E' (69): *v12 -> mfCurrentTimeInAir
struct OncomingCompletedEventView     { f32 mfOncomingDistance; };          // case 'G' (71): *v12
struct TrafficCheckingChainEventView  { s32 miChainLength; };               // case 'J' (74): *v12 -> miCurrentTrafficChain
struct NearMissChainCompleteEventView { s32 miChainLength; bool mbValid; }; // case 'B' (66): *v12 (count) + +4 (valid)
struct PowerParkResultEventView       { s32 miResult; s32 miNumCars; };     // case '6' (54): *v12 (==1) + +4 (cars)

// CompletedStuntEvent (case 'w' (119)). The X360 reads flag word @+0x00, drift @+0x50,
// spin @+0x5C (deg = *0x5C * 57.29578), barrel-roll @+0x68 (flag bit 0x40), air-distance
// @+0x70 (flag bit 0x200).
struct CompletedStuntEventView
{
    u32 muFlags;        // +0x00
    u8  maPad04[0x50 - 0x04];
    f32 mfDriftScore;   // +0x50
    u8  maPad54[0x5C - 0x54];
    f32 mfSpinRadians;  // +0x5C
    u8  maPad60[0x68 - 0x60];
    f32 mfBarrelRoll;   // +0x68
    u8  maPad6C[0x70 - 0x6C];
    f32 mfAirDistance;  // +0x70
};

// The OnlineRoadRulesPersonalBestRecvEvent payload (ProcessNetworkRoadRulePB / PreWorld
// case 139). The X360 copies the leading 7 quadwords (56 bytes) into a stack
// ChallengeHighScoreEntry, then reads mPlayerID @+0x38 and mChallengeIndex @+0x3C, with a
// validity byte @+0x3C of the burnout-skillz event in the PreWorld case.
struct OnlineRoadRulesPersonalBestRecvEventView
{
    u8  maHighScorePayload[0x38]; // a ChallengeHighScoreEntry image (56 bytes)
    s32 mPlayerID;                // +0x38
    s32 mChallengeIndex;          // +0x3C
};

// BurnoutSkillzEvent payload (PreWorld case 139). Player id @+0x00, a 56-byte skillz
// payload @+0x04, validity byte @+0x3C.
struct BurnoutSkillzEventView
{
    s32 mPlayerID;          // +0x00
    u8  maSkillzPayload[56];// +0x04 (56 bytes)
    bool mbHasData;         // +0x3C
};

// PlayerFinalisedEvent payload (PreWorld case 128). Just the network player id @+0x00.
struct PlayerFinalisedEventView { s32 mPlayerID; };

// The burnout-skillz network event the manager (re)broadcasts: a 168-byte (0xA8) record
// of type 68 (0x44). The X360 fills the first 56 bytes from the source skillz data, then
// the trailing fields below (player id, race-car index, a flag, accuracy, road-rule
// time/crash floats). Modelled as the byte image the X360 memcpy + field stores produce.
struct UpdatePlayerSkillsNetEvent
{
    u8  maSkillzData[56];   // +0x00 (56-byte skillz payload copied from source)
    f32 mfRoadRuleTime;     // +0x30 (index 12) -- v35[12] / v86[12]
    f32 mfRoadRuleCrash;    // +0x34 (index 13) -- v35[13] / v86[13]
    s32 miAccuracyOrFlag;   // +0x38 (index 14) -- v35[14] = 4, or v86[14]
    f32 mfV15;              // +0x3C (index 15) -- v86[15]
    s32 miPlayerId;         // +0x40 (index 16)
    s32 miRaceCarIndex;     // +0x44 (index 17)
    u8  maTail[0xA8 - 0x48];
};

// The "road-rule beaten" net event (type 169 / 0x18 bytes): the beaten road id quadword.
struct RoadRuleBeatenNetEvent { u64 mu64RoadID; u8 maPad[0x18 - 8]; };

// The received-road-rules queue (NetworkToGameStateInterface::RoadRulesReceivedQueue). The X360
// reads the entry count at +0x08 and reaches the idx-th RoadRulesRecvData (264 bytes) via
// BrnNetwork::RoadRulesRecvDa(queue, idx). Modelled here as the count + a contiguous entry array
// (the X360 stride is sizeof(RoadRulesRecvData) == 264). No DWARF/leak shape exists for the queue
// header, so the two leading words are opaque storage; the count + entries are X360-attested.
struct RoadRulesRecvQueueView
{
    u8                            maHeader[0x08]; // +0x00 opaque queue header
    s32                           miCount;        // +0x08 received-entry count
    s32                           miPad0C;        // +0x0C padding to the entry array
    BrnNetwork::RoadRulesRecvData maEntries[1];   // +0x10 first entry (flexible run)
};

// Address of the idx-th received road-rules entry (the X360 BrnNetwork::RoadRulesRecvDa helper).
static const BrnNetwork::RoadRulesRecvData*
RoadRulesRecvQueueGetEntry(const GameStateModuleIO::NetworkToGameStateInterface* lpQueue, s32 liIndex)
{
    const RoadRulesRecvQueueView* lpView = reinterpret_cast<const RoadRulesRecvQueueView*>(lpQueue);
    return &lpView->maEntries[liIndex];
}

// ----------------------------------------------------------------------------
// Construct -- X360 0x82332688.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::Construct(ModeManager* lpModeManager)
{
    CGS_ASSERT(lpModeManager, "lpModeManager");

    mpScoringSystem = lpModeManager->GetScoringSystem();
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    // The achievement manager is the subobject embedded in the owning GameStateModule
    // (the X360 reaches it as modeManager->mpGameStateModule + 181680).
    mpAchievementManager = lpModeManager->GetGameStateModule()->GetAchievementManager();
    CGS_ASSERT(mpAchievementManager, "mpAchievementManager");

    mfCurrentTimeInAir = 0.0f;
    mabDirtyFlags.Construct();        // zero the dirty-flag bit set
    miCurrentTrafficChain = 0;
    miCurrentBoostChains = 0;
    mbIsInCrashedState = false;
    for (s32 liActiveRaceCarIndex = 0; liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActiveRaceCarIndex)
    {
        maiBurnoutSkillsUpdatedFlags[liActiveRaceCarIndex] = 0;
    }
    miCurrentRoadIndex = 0;

    mBufferedChallengeScore.Construct();
    meBufferedScoreType = BrnStreetData::E_SCORE_TYPE_COUNT;   // == 2
    mBufferedScoreChallengeIndex = -1;
}

// ----------------------------------------------------------------------------
// ClearAllBurnoutSkillzData -- X360 0x82322480. Reset every car's road-rule scores +
// clear the whole scoring-system skillz tally + zero the per-frame skill scalars/flags.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::ClearAllBurnoutSkillzData()
{
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    for (s32 liScoringIndex = 0; liScoringIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liScoringIndex)
    {
        CarData* lpCarData = mpScoringSystem->GetCarDataFromPlayerScoringIndex(
            static_cast<GameStateModuleIO::EPlayerScoringIndex>(liScoringIndex));
        CGS_ASSERT(lpCarData, "lpCarData");
        lpCarData->ResetRoadRulesScores();
    }

    mpScoringSystem->ClearAllBurnoutSkillzData();

    miCurrentTrafficChain = 0;
    miCurrentBoostChains = 0;
    mfCurrentTimeInAir = 0.0f;
    mabDirtyFlags.Construct();
    for (s32 liActiveRaceCarIndex = 0; liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActiveRaceCarIndex)
    {
        maiBurnoutSkillsUpdatedFlags[liActiveRaceCarIndex] = 0;
    }
    miCurrentRoadIndex = 0;
}

// ----------------------------------------------------------------------------
// OnEnterRoad -- X360 0x82322B98. Record the new road, mark every car's dirty bit, and
// OR the "on a road" flag (bit 4) into each car's updated-flags slot.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::OnEnterRoad(BrnNetwork::Road::ChallengeIndex liRoadIndex)
{
    miCurrentRoadIndex = liRoadIndex;
    mabDirtyFlags.SetAll();
    for (s32 liActiveRaceCarIndex = 0; liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActiveRaceCarIndex)
    {
        maiBurnoutSkillsUpdatedFlags[liActiveRaceCarIndex] |= 4u;
    }
}

// ----------------------------------------------------------------------------
// GetActiveRaceCarIndex -- X360 0x82322B18. Look up the car record for a network player
// id and return its active-race-car index, or E_ACTIVE_RACE_CAR_INDEX_INVALID.
// ----------------------------------------------------------------------------
EActiveRaceCarIndex BurnoutSkillzManager::GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    CarData* lpData = mpScoringSystem->GetCarData(lNetworkPlayerID);
    if (lpData)
    {
        return lpData->GetActiveRaceCarIndex();
    }
    return E_ACTIVE_RACE_CAR_INDEX_INVALID;
}

// ----------------------------------------------------------------------------
// GetRoadRuleHighScore -- X360 0x823227C0. Copy a challenge's stored high-score entry out
// of the network player's car record; returns false if the player has no car record.
// ----------------------------------------------------------------------------
bool BurnoutSkillzManager::GetRoadRuleHighScore(BrnNetwork::NetworkPlayerID lNetworkPlayerID,
                                                BrnNetwork::Road::ChallengeIndex liIndex,
                                                BrnStreetData::ChallengeHighScoreEntry* lpData)
{
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    CarData* lpCarData = mpScoringSystem->GetCarData(lNetworkPlayerID);
    if (lpCarData)
    {
        CGS_ASSERT(lpData, "lpData");
        CGS_ASSERT(liIndex < KI_MAX_CHALLENGES && liIndex >= 0,
                   "liIndex < BrnGameState::KI_MAX_CHALLENGES && liIndex >= 0");
        lpCarData->GetRoadRulesScores(liIndex, lpData);
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// SetRoadRuleHighScore -- X360 0x823228A0. Store a challenge's high-score entry into the
// network player's car record.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::SetRoadRuleHighScore(BrnNetwork::NetworkPlayerID lNetworkPlayerID,
                                                BrnNetwork::Road::ChallengeIndex liIndex,
                                                BrnStreetData::ChallengeHighScoreEntry* lpData)
{
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    CarData* lpCarData = mpScoringSystem->GetCarData(lNetworkPlayerID);
    CGS_ASSERT(lpCarData, "lpCarData");
    CGS_ASSERT(lpData, "lpData");
    CGS_ASSERT(liIndex < KI_MAX_CHALLENGES && liIndex >= 0,
               "liIndex < BrnGameState::KI_MAX_CHALLENGES && liIndex >= 0");
    lpCarData->SetRoadRulesScores(liIndex, lpData);
}

// ----------------------------------------------------------------------------
// SetNewSkillIfGreater -- X360 0x823225C0. If the new value beats the car's stored skill,
// bank it, set the car's dirty bit, and OR the "skills updated" flags (0xE) into its slot.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::SetNewSkillIfGreater(BurnoutSkillzData::EBurnoutSkillType leSkillType,
                                                BurnoutSkillzData* lpSkillzData,
                                                EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex,
                                                f32 lfNewValue)
{
    CGS_ASSERT(lpSkillzData, "lpSkillzData");
    CGS_ASSERT(leSkillType >= BurnoutSkillzData::E_BURNOUT_SKILL_START,
               "leSkillType >= BurnoutSkillzData::E_BURNOUT_SKILL_START");
    CGS_ASSERT(leSkillType < BurnoutSkillzData::E_BURNOUT_SKILL_COUNT,
               "leSkillType < BurnoutSkillzData::E_BURNOUT_SKILL_COUNT");
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
               leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "( leLocalPlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

    if (lfNewValue > lpSkillzData->GetBurnoutSkill(leSkillType))
    {
        lpSkillzData->SetBurnoutSkill(leSkillType, lfNewValue);
        mabDirtyFlags.SetBit(static_cast<u32>(leLocalPlayerActiveRaceCarIndex));
        maiBurnoutSkillsUpdatedFlags[leLocalPlayerActiveRaceCarIndex] |= 0xEu;
    }
}

// ----------------------------------------------------------------------------
// SendUpdatePlayerSkillsEvent -- X360 0x82322988. Mark the car dirty and OR the
// "send update" flags into its slot (bit 4 always, bit 8 when a HUD message is wanted).
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::SendUpdatePlayerSkillsEvent(EActiveRaceCarIndex leActiveRaceCarIndex,
                                                       bool lbShowHudMessage)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    mabDirtyFlags.SetBit(static_cast<u32>(leActiveRaceCarIndex));
    maiBurnoutSkillsUpdatedFlags[leActiveRaceCarIndex] |= 4u;
    if (lbShowHudMessage)
    {
        maiBurnoutSkillsUpdatedFlags[leActiveRaceCarIndex] |= 8u;
    }
}

// ----------------------------------------------------------------------------
// UpdateBoostChains -- X360 0x823327B8. When a boost chain ends (boost-output flag clears),
// bank the accumulated chain count as the BOOST_CHAIN skill and reset the chain counter.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::UpdateBoostChains(
    BurnoutSkillzData* lpSkillzData,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
    EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex)
{
    CGS_ASSERT(lpActiveCarInterface, "lpActiveCarInterface");
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leLocalPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    // GetBoostOutputInfoN takes the global ::EActiveRaceCarIndex; cast across the same-valued
    // dup enum (BrnGameState::EActiveRaceCarIndex here vs the vehicle interface's global one).
    const BoostOutputInfo* lpBoostInfo = lpActiveCarInterface->GetBoostOutputInfoN(
        static_cast<::EActiveRaceCarIndex>(leLocalPlayerActiveRaceCarIndex));
    CGS_ASSERT(lpBoostInfo, "lpBoostInfo");

    // muNumChained (+0x0C): the live boost-chain count. While it is non-zero the chain is
    // still going; when it drops to zero and we have a banked chain count, award it.
    if (lpBoostInfo->muNumChained == 0 && miCurrentBoostChains > 0)
    {
        SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_BOOST_CHAIN, lpSkillzData,
                             leLocalPlayerActiveRaceCarIndex,
                             static_cast<f32>(miCurrentBoostChains));
        miCurrentBoostChains = 0;
    }
}

// ----------------------------------------------------------------------------
// ProcessGameEventInputQueuePostWorld -- X360 0x823328D0. Walk the frame's post-world game
// events and feed completed stunts / near-miss / oncoming / power-park / in-air / traffic
// chains into the per-car skill records.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::ProcessGameEventInputQueuePostWorld(
    BurnoutSkillzData* lpSkillzData,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
    const GameStateModuleIO::GameEventQueue* lpQueue,
    EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex)
{
    CGS_ASSERT(lpSkillzData, "lpSkillzData");
    CGS_ASSERT(lpQueue, "lpQueue");
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leLocalPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const GameEventQueueImpl* lpQueueImpl = reinterpret_cast<const GameEventQueueImpl*>(lpQueue);

    bool lbInAirReceived = false;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liEventSize = 0;
    s32 liEventType = lpQueueImpl->GetFirstEvent(&lpEvent, &liEventSize);
    while (lpEvent)
    {
        switch (liEventType)
        {
        case '6': // PowerParkResultEvent (54)
        {
            const PowerParkResultEventView* lpPowerParkEvent =
                reinterpret_cast<const PowerParkResultEventView*>(lpEvent);
            CGS_ASSERT(lpPowerParkEvent, "lpPowerParkEvent");
            if (lpPowerParkEvent->miResult == 1)
            {
                SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_POWER_PARKING, lpSkillzData,
                                     leLocalPlayerActiveRaceCarIndex,
                                     static_cast<f32>(lpPowerParkEvent->miNumCars));
            }
            break;
        }
        case 'B': // NearMissChainCompleteEvent (66)
        {
            const NearMissChainCompleteEventView* lpNearMissEvent =
                reinterpret_cast<const NearMissChainCompleteEventView*>(lpEvent);
            CGS_ASSERT(lpNearMissEvent, "lpNearMissEvent");
            if (lpNearMissEvent->mbValid)
            {
                SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_NEAR_MISSES, lpSkillzData,
                                     leLocalPlayerActiveRaceCarIndex,
                                     static_cast<f32>(lpNearMissEvent->miChainLength));
            }
            break;
        }
        case 'E': // InAirEvent (69)
        {
            const InAirEventView* lpInAirEvent = reinterpret_cast<const InAirEventView*>(lpEvent);
            CGS_ASSERT(lpInAirEvent, "lpInAirEvent");
            lbInAirReceived = true;
            mfCurrentTimeInAir = lpInAirEvent->mfTimeInAir;
            break;
        }
        case 'G': // OncomingCompletedEvent (71)
        {
            const OncomingCompletedEventView* lpOncomingEvent =
                reinterpret_cast<const OncomingCompletedEventView*>(lpEvent);
            CGS_ASSERT(lpOncomingEvent, "lpOncomingEvent");
            SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_ONCOMING, lpSkillzData,
                                 leLocalPlayerActiveRaceCarIndex, lpOncomingEvent->mfOncomingDistance);
            break;
        }
        case 'J': // TrafficCheckingChainEvent (74)
        {
            const TrafficCheckingChainEventView* lpTrafficCheckEvent =
                reinterpret_cast<const TrafficCheckingChainEventView*>(lpEvent);
            CGS_ASSERT(lpTrafficCheckEvent, "lpEvent");
            miCurrentTrafficChain = lpTrafficCheckEvent->miChainLength;
            break;
        }
        case 'w': // CompletedStuntEvent (119)
        {
            const CompletedStuntEventView* lpCompletedStuntEvent =
                reinterpret_cast<const CompletedStuntEventView*>(lpEvent);
            CGS_ASSERT(lpCompletedStuntEvent, "lpCompletedStuntEvent");
            SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_SPIN, lpSkillzData,
                                 leLocalPlayerActiveRaceCarIndex,
                                 lpCompletedStuntEvent->mfSpinRadians * 57.29578f);
            SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_BARREL_ROLL, lpSkillzData,
                                 leLocalPlayerActiveRaceCarIndex, lpCompletedStuntEvent->mfDriftScore);
            if ((lpCompletedStuntEvent->muFlags & 0x40) == 0x40)
            {
                SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_DRIFT, lpSkillzData,
                                     leLocalPlayerActiveRaceCarIndex, lpCompletedStuntEvent->mfBarrelRoll);
            }
            if ((lpCompletedStuntEvent->muFlags & 0x200) == 0x200)
            {
                SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_AIR_DISTANCE, lpSkillzData,
                                     leLocalPlayerActiveRaceCarIndex, lpCompletedStuntEvent->mfAirDistance);
            }
            break;
        }
        default:
            break;
        }

        liEventType = lpQueueImpl->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
    }

    // While the player is being reset (post-crash) or hidden, drop any pending in-air time.
    const RaceCarState* lpRaceCarState = lpActiveCarInterface->GetPlayerRaceCarState();
    if (lpRaceCarState->mbResetCarTransform || lpRaceCarState->mbIsHidden)
    {
        mfCurrentTimeInAir = 0.0f;
    }

    // No in-air event this frame -> the air time just ended; bank it as the AIR_TIME skill.
    if (!lbInAirReceived)
    {
        if (mfCurrentTimeInAir > 0.0f)
        {
            SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_AIR_TIME, lpSkillzData,
                                 leLocalPlayerActiveRaceCarIndex, mfCurrentTimeInAir);
            mfCurrentTimeInAir = 0.0f;
        }
    }
}

// ----------------------------------------------------------------------------
// ProcessNewRoadScore -- X360 0x82345348. A buffered road score is finalised: tell the
// mugshot manager if it beat someone, push the "road rule beaten" + "set" game actions,
// update the local car's road-rule high-score table, broadcast the score, and (for the
// local player) mark the car dirty.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::ProcessNewRoadScore(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                               BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                                               BrnStreetData::ScoreType leScoreType,
                                               BrnNetwork::Road::ChallengeIndex liChallengeIndex,
                                               EActiveRaceCarIndex leLocalActiveRaceCarIndex)
{
    CGS_ASSERT(leLocalActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
               leLocalActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "( leLocalActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leLocalActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    s32                 liPreviousHolderScore = 0;
    EActiveRaceCarIndex lePreviousHolderRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    mpScoringSystem->GetHighestLobbyRoadRuleScore(liChallengeIndex, leScoreType,
                                                  &liPreviousHolderScore, &lePreviousHolderRaceCarIndex);

    const s32 liNewScore = lChallengeScore.GetScore(leScoreType);

    if (lChallengeScore.CompareScores(leScoreType, liNewScore, liPreviousHolderScore) < 0)
    {
        if (lePreviousHolderRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
            lePreviousHolderRaceCarIndex != leLocalActiveRaceCarIndex)
        {
            CGS_ASSERT(mpMugshotManager, "mpMugshotManager");
            const BrnStreetData::Road* lpRoad =
                mpStreetManager->GetStreetData()->GetRoad(liChallengeIndex);
            // ProcessBeatenRoadRuleEvent takes the global ::EActiveRaceCarIndex (cast across the dup).
            mpMugshotManager->ProcessBeatenRoadRuleEvent(
                lpOutputBuffer, static_cast<::EActiveRaceCarIndex>(lePreviousHolderRaceCarIndex),
                lpRoad->GetId(), leScoreType);
        }

        CGS_ASSERT(mpStreetManager, "mpStreetManager");

        // "Road rule beaten" action (type 169) carries the beaten road id.
        const BrnStreetData::Road* lpRoad =
            mpStreetManager->GetStreetData()->GetRoad(liChallengeIndex);
        RoadRuleBeatenNetEvent lBeatenEvent;
        lBeatenEvent.mu64RoadID = lpRoad->GetId();
        CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
        CGS_ASSERT(lpOutputBuffer->GetGameActionQueue(), "lpOutputBuffer->GetGameActionQueue()");
        reinterpret_cast<GameActionQueueImpl*>(lpOutputBuffer->GetGameActionQueue())
            ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lBeatenEvent), 169, 24);

        // "Set road rule" notification action (type 149).
        s32 liSetRoadRuleNotification = 44;
        reinterpret_cast<GameActionQueueImpl*>(lpOutputBuffer->GetGameActionQueue())
            ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liSetRoadRuleNotification), 149, 4);
    }

    // Build a high-score entry from the buffered score and merge it into the car record.
    BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
    lHighScoreEntry.Construct(&lChallengeScore);

    CarData* lpCarData = mpScoringSystem->GetCarData(leLocalActiveRaceCarIndex);
    if (lpCarData)
    {
        BrnStreetData::ChallengeHighScoreEntry lExistingEntry;
        lpCarData->GetRoadRulesScores(liChallengeIndex, &lExistingEntry);
        const bool lbUpdated = lExistingEntry.UpdateEntry(&lHighScoreEntry, nullptr);
        lpCarData->SetRoadRulesScores(liChallengeIndex, &lExistingEntry);

        if (lbUpdated)
        {
            // Broadcast the new player score (event type 233 / 0x30 bytes).
            BrnStreetData::ChallengePlayerScoreEntry lBroadcastScore;
            lBroadcastScore.Copy(&lChallengeScore);
            CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
            reinterpret_cast<GameActionQueueImpl*>(lpOutputBuffer->GetGameActionQueue())
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lBroadcastScore), 233, 48);

            if (liChallengeIndex == miCurrentRoadIndex)
            {
                mabDirtyFlags.SetBit(static_cast<u32>(leLocalActiveRaceCarIndex));
                maiBurnoutSkillsUpdatedFlags[leLocalActiveRaceCarIndex] |= 4u;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// ProcessNetworkRoadRulePB -- X360 0x82345740. A remote player's road-rule personal best
// arrived: compare it against the lobby high score, notify on a tie, push the beaten event,
// merge into the player's record, and mark the local car for a skill update.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::ProcessNetworkRoadRulePB(
    GameStateModuleIO::GameActionQueue* lpActionQueue,
    const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent* lpPersonalBestEvent,
    EActiveRaceCarIndex leLocalPlayersActiveRaceCarIndex)
{
    CGS_ASSERT(lpPersonalBestEvent, "lpPersonalBestEvent");

    const OnlineRoadRulesPersonalBestRecvEventView* lpEventView =
        reinterpret_cast<const OnlineRoadRulesPersonalBestRecvEventView*>(lpPersonalBestEvent);
    GameActionQueueImpl* lpActionQueueImpl = reinterpret_cast<GameActionQueueImpl*>(lpActionQueue);

    // The leading 56 bytes are a ChallengeHighScoreEntry image.
    BrnStreetData::ChallengeHighScoreEntry lReceivedEntry;
    std::memcpy(&lReceivedEntry, lpEventView->maHighScorePayload, 56);

    const BrnNetwork::Road::ChallengeIndex liChallengeIndex = lpEventView->mChallengeIndex;
    CGS_ASSERT(liChallengeIndex < KI_MAX_CHALLENGES && liChallengeIndex >= 0,
               "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES && lChallengeIndex >= 0");

    for (s32 liScoreType = BrnStreetData::E_SCORE_TYPE_START;
         liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liScoreType)
    {
        const BrnStreetData::ScoreType leScoreType = static_cast<BrnStreetData::ScoreType>(liScoreType);
        if (lReceivedEntry.ContainsData(leScoreType))
        {
            CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

            s32                 liHighScore = 0;
            EActiveRaceCarIndex leHighScoreRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
            mpScoringSystem->GetHighestLobbyRoadRuleScore(liChallengeIndex, leScoreType,
                                                          &liHighScore, &leHighScoreRaceCarIndex);

            s32 liReceivedScore = 0;
            CgsNetwork::PlayerName lReceivedName;
            lReceivedEntry.GetScore(leScoreType, &liReceivedScore, &lReceivedName);

            if (lReceivedEntry.CompareScores(leScoreType, liReceivedScore, liHighScore) < 0)
            {
                EActiveRaceCarIndex leSenderRaceCarIndex = GetActiveRaceCarIndex(lpEventView->mPlayerID);
                if (leSenderRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                {
                    CGS_ASSERT(mpStreetManager, "mpStreetManager");

                    // Push a "set road rule" notification: type 149 (tie -> notify) or the
                    // local "set" path (a flag word). The X360 selects the score-type index
                    // 12 or 13 (time vs crash) and notes whether the sender already holds it.
                    s32 liSetFlag;
                    if (leLocalPlayersActiveRaceCarIndex == leHighScoreRaceCarIndex)
                    {
                        liSetFlag = 0;
                        s32 liNotification = 45;
                        lpActionQueueImpl->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&liNotification), 149, 4);
                    }
                    else
                    {
                        liSetFlag = 1;
                    }
                    (void)liSetFlag;

                    const BrnStreetData::Road* lpRoad =
                        mpStreetManager->GetStreetData()->GetRoad(liChallengeIndex);
                    RoadRuleBeatenNetEvent lBeatenEvent;
                    lBeatenEvent.mu64RoadID = lpRoad->GetId();
                    CGS_ASSERT(lpActionQueue, "lpActionQueue");
                    lpActionQueueImpl->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lBeatenEvent), 169, 24);
                }
            }
        }
    }

    // Merge the received entry into the sender's car record (build a fresh blank entry first).
    BrnStreetData::ChallengeHighScoreEntry lStoredEntry;
    lStoredEntry.Construct();
    if (GetRoadRuleHighScore(lpEventView->mPlayerID, liChallengeIndex, &lStoredEntry))
    {
        lStoredEntry.UpdateEntry(&lReceivedEntry, nullptr);
        SetRoadRuleHighScore(lpEventView->mPlayerID, liChallengeIndex, &lStoredEntry);
    }

    // If the sender's record is for the current road, kick a skill update for them.
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
    CarData* lpSenderCar = mpScoringSystem->GetCarData(lpEventView->mPlayerID);
    if (lpSenderCar)
    {
        if (lpSenderCar->GetActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
            liChallengeIndex == miCurrentRoadIndex)
        {
            EActiveRaceCarIndex leSenderRaceCarIndex = GetActiveRaceCarIndex(lpEventView->mPlayerID);
            if (leSenderRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                CGS_ASSERT(leSenderRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leNetworkPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT");
                SendUpdatePlayerSkillsEvent(leSenderRaceCarIndex, false);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// ProcessGameEventInputQueuePreWorld -- X360 0x82345AC0. Walk the frame's pre-world game
// events: clear-all, player finalised, network road-rule PB, and the burnout-skillz event.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::ProcessGameEventInputQueuePreWorld(
    const GameStateModuleIO::GameEventQueue* lpEventQueue,
    GameStateModuleIO::GameActionQueue* lpActionQueue,
    EActiveRaceCarIndex leLocalPlayersActiveRaceCarIndex)
{
    CGS_ASSERT(lpActionQueue, "lpActionQueue");
    CGS_ASSERT(lpEventQueue, "lpEventQueue");
    CGS_ASSERT(leLocalPlayersActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leLocalPlayersActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leLocalPlayersActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leLocalPlayersActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const GameEventQueueImpl* lpEventQueueImpl = reinterpret_cast<const GameEventQueueImpl*>(lpEventQueue);
    GameActionQueueImpl*      lpActionQueueImpl = reinterpret_cast<GameActionQueueImpl*>(lpActionQueue);

    const CgsModule::Event* lpEvent = nullptr;
    s32 liEventSize = 0;
    s32 liEventType = lpEventQueueImpl->GetFirstEvent(&lpEvent, &liEventSize);
    while (lpEvent)
    {
        switch (liEventType)
        {
        case 124: // clear-all
            ClearAllBurnoutSkillzData();
            break;

        case 128: // player finalised
        {
            const PlayerFinalisedEventView* lpPlayerFinalisedEvent =
                reinterpret_cast<const PlayerFinalisedEventView*>(lpEvent);
            CGS_ASSERT(lpPlayerFinalisedEvent, "lpPlayerFinalisedEvent");

            CarData* lpCarData = mpScoringSystem->GetCarData(lpPlayerFinalisedEvent->mPlayerID);
            CGS_ASSERT(mpScoringSystem->GetBurnoutSkillzData(lpPlayerFinalisedEvent->mPlayerID),
                       "Can't find player in burnout skills data");
            lpCarData->ResetRoadRulesScores();

            BurnoutSkillzData* lpSkillzData =
                mpScoringSystem->GetBurnoutSkillzData(leLocalPlayersActiveRaceCarIndex);
            CarData* lpLocalCarData = mpScoringSystem->GetCarData(leLocalPlayersActiveRaceCarIndex);
            CGS_ASSERT(lpSkillzData, "lpSkillzData");
            if (lpLocalCarData)
            {
                CGS_ASSERT(lpSkillzData, "lpCarData");
                // Rebroadcast the local player's skillz totals (event type 68 / 168 bytes).
                UpdatePlayerSkillsNetEvent lNetEvent;
                std::memcpy(lNetEvent.maSkillzData, lpSkillzData, sizeof(lNetEvent.maSkillzData));
                lNetEvent.miPlayerId = lpPlayerFinalisedEvent->mPlayerID;
                lNetEvent.miRaceCarIndex = leLocalPlayersActiveRaceCarIndex;
                lpActionQueueImpl->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNetEvent), 68, 168);
            }
            break;
        }

        case 130: // online road-rule personal best
            ProcessNetworkRoadRulePB(
                lpActionQueue,
                reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent*>(lpEvent),
                leLocalPlayersActiveRaceCarIndex);
            break;

        case 139: // burnout-skillz event
        {
            const BurnoutSkillzEventView* lpBurnoutSkillzEvent =
                reinterpret_cast<const BurnoutSkillzEventView*>(lpEvent);
            CGS_ASSERT(lpBurnoutSkillzEvent, "lpBurnoutSkillzEvent");

            EActiveRaceCarIndex leNetworkPlayerActiveRaceCarIndex =
                GetActiveRaceCarIndex(lpBurnoutSkillzEvent->mPlayerID);
            if (leNetworkPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                CGS_ASSERT(leNetworkPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leNetworkPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT");

                if (lpBurnoutSkillzEvent->mbHasData)
                {
                    UpdatePlayerSkillsNetEvent lNetEvent;
                    lNetEvent.miPlayerId = lpBurnoutSkillzEvent->mPlayerID;
                    lNetEvent.miRaceCarIndex = leNetworkPlayerActiveRaceCarIndex;
                    std::memcpy(lNetEvent.maSkillzData, lpBurnoutSkillzEvent->maSkillzPayload, 56);
                    lNetEvent.miAccuracyOrFlag = 4;

                    BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
                    GetRoadRuleHighScore(lpBurnoutSkillzEvent->mPlayerID, miCurrentRoadIndex, &lHighScoreEntry);
                    if (lHighScoreEntry.ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                    {
                        s32 liScore = 0;
                        CgsNetwork::PlayerName lName;
                        lHighScoreEntry.GetScore(BrnStreetData::E_SCORE_TYPE_TIME, &liScore, &lName);
                        f32 lfRoundedTime = static_cast<f32>(liScore) * 0.001f;
                        BrnNetwork::NetworkRounder::RoundFloatToAccuracy(&lfRoundedTime, 0.0049999999);
                        lNetEvent.mfRoadRuleTime = lfRoundedTime;
                    }
                    if (lHighScoreEntry.ContainsData(BrnStreetData::E_SCORE_TYPE_CRASH))
                    {
                        s32 liScore = 0;
                        CgsNetwork::PlayerName lName;
                        lHighScoreEntry.GetScore(BrnStreetData::E_SCORE_TYPE_CRASH, &liScore, &lName);
                        f32 lfRoundedCrash = static_cast<f32>(liScore);
                        BrnNetwork::NetworkRounder::RoundFloatToAccuracy(&lfRoundedCrash, 0.0049999999);
                        lNetEvent.mfRoadRuleCrash = lfRoundedCrash;
                    }
                    lpActionQueueImpl->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNetEvent), 68, 168);
                }
                else
                {
                    SendUpdatePlayerSkillsEvent(leNetworkPlayerActiveRaceCarIndex, true);
                }
            }
            break;
        }

        default:
            break;
        }

        liEventType = lpEventQueueImpl->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
    }
}

// ----------------------------------------------------------------------------
// UpdateLobbyRoadRulesScores -- X360 0x8233A668. Drain the received road-rules queue,
// merging each remote player's scores into our records and kicking skill updates.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::UpdateLobbyRoadRulesScores(
    const GameStateModuleIO::NetworkToGameStateInterface* lpRoadRulesRecvQueue)
{
    CGS_ASSERT(lpRoadRulesRecvQueue, "lpRoadRulesRecvQueue");

    // The interface front-loads a count (X360 *(queue+8)); each entry is a RoadRulesRecvData.
    const RoadRulesRecvQueueView* lpQueueView =
        reinterpret_cast<const RoadRulesRecvQueueView*>(lpRoadRulesRecvQueue);

    for (s32 liEntry = 0; liEntry < lpQueueView->miCount; ++liEntry)
    {
        BrnNetwork::RoadRulesRecvData lRecvData;
        std::memcpy(&lRecvData, RoadRulesRecvQueueGetEntry(lpRoadRulesRecvQueue, liEntry), 264);

        if (lRecvData.miNumRoadRulesScoresRecv > 0)
        {
            const BrnNetwork::NetworkPlayerID lPlayerID = lRecvData.mPlayerID;
            CgsNetwork::PlayerName lPlayerName;
            std::memcpy(&lPlayerName, &lRecvData.mPlayerName, sizeof(lPlayerName));

            for (s32 liScoreEntry = 0; liScoreEntry < lRecvData.miNumRoadRulesScoresRecv; ++liScoreEntry)
            {
                const BrnNetwork::RoadRulesMessageData& lMessage = lRecvData.maRoadRulesData[liScoreEntry];
                const u32 luChallengeIndex = static_cast<u32>(lMessage.mChallengeIndex);
                CGS_ASSERT(luChallengeIndex < static_cast<u32>(KI_MAX_CHALLENGES), "Invalid Road Index");

                // Build a high-score entry from the received per-type scores.
                BrnStreetData::ChallengeHighScoreEntry lReceivedEntry;
                lReceivedEntry.Construct();
                for (s32 liScoreType = BrnStreetData::E_SCORE_TYPE_START;
                     liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liScoreType)
                {
                    const BrnStreetData::ScoreType leScoreType =
                        static_cast<BrnStreetData::ScoreType>(liScoreType);
                    if (lMessage.maScores[liScoreType])
                    {
                        lReceivedEntry.SetScore(leScoreType, lMessage.maScores[liScoreType], &lPlayerName);
                    }
                }

                BrnStreetData::ChallengeHighScoreEntry lStoredEntry;
                lStoredEntry.Construct();
                if (GetRoadRuleHighScore(lPlayerID, static_cast<BrnNetwork::Road::ChallengeIndex>(luChallengeIndex), &lStoredEntry))
                {
                    lStoredEntry.UpdateEntry(&lReceivedEntry, nullptr);
                    SetRoadRuleHighScore(lPlayerID, static_cast<BrnNetwork::Road::ChallengeIndex>(luChallengeIndex), &lStoredEntry);
                }

                if (luChallengeIndex == static_cast<u32>(miCurrentRoadIndex))
                {
                    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
                    CarData* lpCarData = mpScoringSystem->GetCarData(lPlayerID);
                    if (lpCarData)
                    {
                        EActiveRaceCarIndex leRaceCarIndex = lpCarData->GetActiveRaceCarIndex();
                        if (leRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                        {
                            CGS_ASSERT(leRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT,
                                       "leNetworkPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT");
                            SendUpdatePlayerSkillsEvent(leRaceCarIndex, false);
                        }
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateBurnoutSkillzTotals -- X360 0x82322C30. Recompute the "best of" per-skill totals
// across all active cars, award the per-skill leader a point, and (when a total changes)
// mark the car dirty + notify the achievement manager for the local player.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::UpdateBurnoutSkillzTotals(EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex)
{
    // Per-skill winner tracking across the SEND-VIA-NETWORK skill range (11 skills).
    const s32 KI_SKILL_RANGE = BurnoutSkillzData::E_BURNOUT_SKILL_TO_SEND_VIA_NETWORK_COUNT + 2; // 11

    bool                labIsTie[KI_SKILL_RANGE];
    f32                 lafBestValue[KI_SKILL_RANGE];
    EActiveRaceCarIndex laeBestCar[KI_SKILL_RANGE];
    for (s32 liSkill = 0; liSkill < KI_SKILL_RANGE; ++liSkill)
    {
        labIsTie[liSkill] = false;
        lafBestValue[liSkill] = 0.0f;
        laeBestCar[liSkill] = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // Per-car running point totals.
    f32 lafTotals[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        lafTotals[liCar] = 0.0f;
    }

    // Gather each car's skillz record.
    BurnoutSkillzData* lapSkillzData[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        lapSkillzData[liCar] =
            mpScoringSystem->GetBurnoutSkillzData(static_cast<EActiveRaceCarIndex>(liCar));
    }

    // Find the leader (and ties) for each skill.
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        BurnoutSkillzData* lpSkillzData = lapSkillzData[liCar];
        if (lpSkillzData)
        {
            for (s32 liSkill = 0; liSkill < KI_SKILL_RANGE; ++liSkill)
            {
                const f32 lfValue =
                    lpSkillzData->GetBurnoutSkill(static_cast<BurnoutSkillzData::EBurnoutSkillType>(liSkill));
                if (lfValue > 0.0f)
                {
                    if (lfValue == lafBestValue[liSkill] && laeBestCar[liSkill] != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                    {
                        labIsTie[liSkill] = true;
                    }
                    if (lfValue > lafBestValue[liSkill] || laeBestCar[liSkill] == E_ACTIVE_RACE_CAR_INDEX_INVALID)
                    {
                        lafBestValue[liSkill] = lfValue;
                        laeBestCar[liSkill] = static_cast<EActiveRaceCarIndex>(liCar);
                        labIsTie[liSkill] = false;
                    }
                }
            }
        }
    }

    // Award an outright leader (non-tie) one point per skill.
    for (s32 liSkill = 0; liSkill < KI_SKILL_RANGE; ++liSkill)
    {
        if (laeBestCar[liSkill] != E_ACTIVE_RACE_CAR_INDEX_INVALID && !labIsTie[liSkill])
        {
            lafTotals[laeBestCar[liSkill]] += 1.0f;
        }
    }

    // Push any changed total back into the per-car skillz record (rounded), mark dirty, and
    // notify the achievement manager for the local player.
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        BurnoutSkillzData* lpSkillzData = lapSkillzData[liCar];
        if (lpSkillzData)
        {
            const f32 lfNewTotal = lafTotals[liCar];
            const f32 lfStoredTotal =
                lpSkillzData->GetBurnoutSkill(BurnoutSkillzData::E_BURNOUT_SKILL_TOTAL);
            if (lfNewTotal != lfStoredTotal)
            {
                f32 lfRounded = lafTotals[liCar];
                BrnNetwork::NetworkRounder::RoundFloatToAccuracy(&lfRounded, 0.0049999999);
                lpSkillzData->SetBurnoutSkill(BurnoutSkillzData::E_BURNOUT_SKILL_TOTAL, lfRounded);

                mabDirtyFlags.SetBit(static_cast<u32>(liCar));
                maiBurnoutSkillsUpdatedFlags[liCar] |= 4u;

                if (liCar == leLocalPlayerActiveRaceCarIndex)
                {
                    const s32 liNumberOfNetworkPlayers = mpScoringSystem->GetNumberOfNetworkPlayers();
                    mpAchievementManager->OnFreeburnSkillzTotalChange(liNumberOfNetworkPlayers, lfNewTotal);
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// PreWorldUpdate -- X360 0x8234D410. The per-frame pre-world entry point: drain the input
// queues, flush any buffered road score, run boost-chain / skillz-total updates while the
// player is alive and grounded, and finally broadcast every dirty car's skillz update.
// ----------------------------------------------------------------------------
void BurnoutSkillzManager::PreWorldUpdate(
    const GameStateModuleIO::PreWorldInputBuffer* lpInput,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
    GameStateModuleIO::OutputBuffer* lpOutput,
    bool lbExitingFreeburnLobby)
{
    CGS_ASSERT(lpActiveCarInterface, "lpActiveCarInterface");
    CGS_ASSERT(lpInput, "lpInput");
    CGS_ASSERT(lpActiveCarInterface, "lpActiveCarInterface");

    // GetPlayerActiveRaceCarIndex returns the global ::EActiveRaceCarIndex; cast across the
    // same-valued dup enum into this TU's BrnGameState::EActiveRaceCarIndex.
    EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex =
        static_cast<EActiveRaceCarIndex>(lpActiveCarInterface->GetPlayerActiveRaceCarIndex());
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "Player car index hasn't been set");
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
               leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "( leLocalPlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

    ProcessGameEventInputQueuePreWorld(lpInput->GetGameEventQueue(), lpOutput->GetGameActionQueue(),
                                       leLocalPlayerActiveRaceCarIndex);
    UpdateLobbyRoadRulesScores(lpInput->GetNetworkToGameStateInterface());

    // Flush a buffered road score (unless we are leaving the lobby).
    if (!lbExitingFreeburnLobby && mBufferedScoreChallengeIndex != -1)
    {
        CGS_ASSERT(meBufferedScoreType != BrnStreetData::E_SCORE_TYPE_COUNT,
                   "meBufferedScoreType != BrnStreetData::E_SCORE_TYPE_COUNT");
        CGS_ASSERT(mBufferedChallengeScore.ContainsData(BrnStreetData::E_SCORE_TYPE_COUNT),
                   "mBufferedChallengeScore.ContainsData()");
        ProcessNewRoadScore(lpOutput, mBufferedChallengeScore, meBufferedScoreType,
                            mBufferedScoreChallengeIndex, leLocalPlayerActiveRaceCarIndex);
        mBufferedChallengeScore.Construct();
        mBufferedScoreChallengeIndex = -1;
        meBufferedScoreType = BrnStreetData::E_SCORE_TYPE_COUNT;
    }

    const RaceCarState* lpRaceCarState = lpActiveCarInterface->GetPlayerRaceCarState();
    CGS_ASSERT(lpRaceCarState, "lpRaceCarState");

    if (lpRaceCarState->mbResetCarTransform)
    {
        miCurrentTrafficChain = 0;
        miCurrentBoostChains = 0;
        mfCurrentTimeInAir = 0.0f;
        mbIsInCrashedState = true;
        return;
    }

    if (mbIsInCrashedState)
    {
        // Wait until all four wheels are back on the ground before resuming skill earning.
        const bool lbAllWheelsOnGround = lpRaceCarState->maWheels[0].mRoadContact.mbIsOnGround &&
                                         lpRaceCarState->maWheels[1].mRoadContact.mbIsOnGround &&
                                         lpRaceCarState->maWheels[2].mRoadContact.mbIsOnGround &&
                                         lpRaceCarState->maWheels[3].mRoadContact.mbIsOnGround;
        if (!lbAllWheelsOnGround)
        {
            return;
        }
        mbIsInCrashedState = false;
    }

    BurnoutSkillzData* lpLocalSkillzData = mpScoringSystem->GetBurnoutSkillzData(leLocalPlayerActiveRaceCarIndex);
    if (lpLocalSkillzData)
    {
        UpdateBoostChains(lpLocalSkillzData, lpActiveCarInterface, leLocalPlayerActiveRaceCarIndex);
    }

    // If any car has a pending skill-update flag, recompute the cross-car totals.
    bool lbAnyUpdatePending = false;
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        if (maiBurnoutSkillsUpdatedFlags[liCar] != 0)
        {
            lbAnyUpdatePending = true;
            break;
        }
    }
    if (!lbAnyUpdatePending)
    {
        UpdateBurnoutSkillzTotals(leLocalPlayerActiveRaceCarIndex);
    }

    // Broadcast every dirty car's skillz update (event type 68 / 168 bytes), clearing its bit.
    for (s32 liDirtyCar = mabDirtyFlags.GetFirstBitSet();
         liDirtyCar != CgsContainers::FastBitArray<E_ACTIVE_RACE_CAR_INDEX_COUNT>::KI_INVALID_BIT_INDEX;
         liDirtyCar = mabDirtyFlags.GetNextBitSet(liDirtyCar))
    {
        const CarData* lpCarData =
            mpScoringSystem->GetCarData(static_cast<EActiveRaceCarIndex>(liDirtyCar));
        BurnoutSkillzData* lpSkillzData =
            mpScoringSystem->GetBurnoutSkillzData(static_cast<EActiveRaceCarIndex>(liDirtyCar));
        if (lpSkillzData)
        {
            const BrnNetwork::NetworkPlayerID lPlayerID = lpCarData->GetNetworkPlayerID();

            UpdatePlayerSkillsNetEvent lNetEvent;
            std::memcpy(lNetEvent.maSkillzData, lpSkillzData, sizeof(lNetEvent.maSkillzData));
            lNetEvent.miPlayerId = lPlayerID;
            lNetEvent.miRaceCarIndex = liDirtyCar;
            lNetEvent.miAccuracyOrFlag = maiBurnoutSkillsUpdatedFlags[liDirtyCar];
            maiBurnoutSkillsUpdatedFlags[liDirtyCar] = 0;

            BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
            if (GetRoadRuleHighScore(lPlayerID, miCurrentRoadIndex, &lHighScoreEntry))
            {
                if (lHighScoreEntry.ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    s32 liScore = 0;
                    CgsNetwork::PlayerName lName;
                    lHighScoreEntry.GetScore(BrnStreetData::E_SCORE_TYPE_TIME, &liScore, &lName);
                    f32 lfRoundedTime = static_cast<f32>(liScore) * 0.001f;
                    BrnNetwork::NetworkRounder::RoundFloatToAccuracy(&lfRoundedTime, 0.0049999999);
                    lNetEvent.mfRoadRuleTime = lfRoundedTime;
                }
                if (lHighScoreEntry.ContainsData(BrnStreetData::E_SCORE_TYPE_CRASH))
                {
                    s32 liScore = 0;
                    CgsNetwork::PlayerName lName;
                    lHighScoreEntry.GetScore(BrnStreetData::E_SCORE_TYPE_CRASH, &liScore, &lName);
                    f32 lfRoundedCrash = static_cast<f32>(liScore);
                    BrnNetwork::NetworkRounder::RoundFloatToAccuracy(&lfRoundedCrash, 0.0049999999);
                    lNetEvent.mfRoadRuleCrash = lfRoundedCrash;
                }
            }

            mabDirtyFlags.UnSetBit(static_cast<u32>(liDirtyCar));
            reinterpret_cast<GameActionQueueImpl*>(lpOutput->GetGameActionQueue())
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNetEvent), 68, 168);
        }
    }
}

// ----------------------------------------------------------------------------
// PostWorldUpdate -- X360 0x8233A560. The per-frame post-world entry point. Resolves the
// local player's active-race-car index from the post-world input buffer's active-car output
// interface, looks up that car's skillz record, feeds the frame's post-world game events into
// it, then banks the car's takedown count as the TOTAL skill. Returns the skillz record it
// operated on (null when the scoring system has no record for the index).
// ----------------------------------------------------------------------------
BurnoutSkillzData* BurnoutSkillzManager::PostWorldUpdate(
    const GameStateModuleIO::PostWorldInputBuffer* lpInput)
{
    CGS_ASSERT(lpInput, "lpInput");

    // The active-car output interface lives inside the post-world input buffer; its
    // GetPlayerActiveRaceCarIndex returns the global ::EActiveRaceCarIndex -- cast across the
    // same-valued dup enum into this TU's BrnGameState::EActiveRaceCarIndex.
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface =
        lpInput->GetActiveRaceCarOutputInterface();
    EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex =
        static_cast<EActiveRaceCarIndex>(lpActiveCarInterface->GetPlayerActiveRaceCarIndex());
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "Player car index hasn't been set");

    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
    BurnoutSkillzData* lpSkillzData =
        mpScoringSystem->GetBurnoutSkillzData(leLocalPlayerActiveRaceCarIndex);
    if (lpSkillzData)
    {
        ProcessGameEventInputQueuePostWorld(lpSkillzData, lpInput->GetActiveRaceCarOutputInterface(),
                                            lpInput->GetGameEventQueue(),
                                            leLocalPlayerActiveRaceCarIndex);

        // Bank the car's takedown count (CarScoreData +0x4C) as the TOTAL skill.
        CarData* lpCarData = mpScoringSystem->GetCarData(leLocalPlayerActiveRaceCarIndex);
        if (lpCarData)
        {
            const s32 liTakedowns = lpCarData->GetScoreData()->GetTakedowns();
            SetNewSkillIfGreater(BurnoutSkillzData::E_BURNOUT_SKILL_TOTAL, lpSkillzData,
                                 leLocalPlayerActiveRaceCarIndex, static_cast<f32>(liTakedowns));
        }
    }

    return lpSkillzData;
}

} // namespace BrnGameState
