#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h" // CgsContainers::FastBitArray
#include "GameSource/GameState/FlybyManager/BrnGameStateFlybyManager.h"              // BrnGameState::FlybyManager (base), CombinedStringID
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusInterface / InGamePlayerStatusData

// ===========================================================================
// BrnGameState::OnlineFlybyManager
//
// The online flyby / rival-rating subsystem (DWARF home
// GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h). It derives from
// BrnGameState::FlybyManager and, for each connected network player, computes a "rivalry
// weighting" used to pick which rivals appear in the pre-race flyby and which flavour of
// rivalry message is shown.
//
// Its own members follow the base: the copy of the network module's in-game player status
// interface (console +0x2A0, the 0x9F0-byte interface, so its player count sits at +0xC84) and the
// "no rivalry" message bit array (console +0xC90; Construct and Prepare both clear it).
// Every per-player lookup is the interface's GetPlayerStatusDataByPlayerID linear search, which
// the console inlines at each site.
// ===========================================================================

namespace BrnGameState
{
class GameStateModule;
class ScoringSystem;

namespace GameStateModuleIO { struct FlybyData; }

// ===========================================================================
// OnlineFlybyManager : public FlybyManager
// ===========================================================================
class OnlineFlybyManager : public FlybyManager
{
public:
    // ---- rivalry-stat comparison enums (DWARF BrnGameStateOnlineFlybyManager.h) ----
    enum ENoRivalryCompare : s32
    {
        E_NO_RIVALRY_START        = 0,
        E_NO_RIVALRY_TAKEDOWNS    = 0,
        E_NO_RIVALRY_TOTAL_RIVALS = 1,
        E_NO_RIVALRY_RANK         = 2,
        E_NO_RIVALRY_EVENTS       = 3,
        E_NO_RIVALRY_WINS         = 4,
        E_NO_RIVALRY_COUNT        = 5,
    };

    enum ENewRivalryCompare : s32
    {
        E_NEW_RIVALRY_START          = 0,
        E_NEW_RIVALRY_CURRENT_STATUS = 0,
        E_NEW_RIVALRY_SCORE_SETTLED  = 1,
        E_NEW_RIVALRY_EVENTS         = 2,
        E_NEW_RIVALRY_LONGEST_STREAK = 3,
        E_NEW_RIVALRY_COUNT          = 4,
    };

    enum EOngoingRivalryCompare : s32
    {
        E_ONGOING_RIVALRY_START               = 0,
        E_ONGOING_RIVALRY_MUGSHOTS_COLLECTED  = 0,
        E_ONGOING_RIVALRY_MARKS               = 1,
        E_ONGOING_RIVALRY_LONGEST_STREAK      = 2,
        E_ONGOING_RIVALRY_WINS                = 3,
        E_ONGOING_RIVALRY_PAYBACKS_SUCCESSFUL = 4,
        E_ONGOING_RIVALRY_PAYBACKS_ESCAPED    = 5,
        E_ONGOING_RIVALRY_COUNT               = 6,
    };

    enum EMessageStyle : s32
    {
        E_MESSAGE_STYLE_POSITIVE = 0,
        E_MESSAGE_STYLE_NEGATIVE = 1,
        E_MESSAGE_STYLE_COUNT    = 2,
    };

    // ---- lifecycle (reference BrnGameStateOnlineFlybyManager.cpp). The console inlines both
    // into GameStateModule::Construct and GameStateModule::Prepare's stage 18. ----
    void Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem);
    bool Prepare();

    // Reference BrnGameStateOnlineFlybyManager.h, header inline: reseeds the base random generator
    // with the network game's shared seed so every machine draws the same flyby.
    void SetRandomNetworkGameSeed(u32 luSeed)
    {
        GetRandom()->SetSeed(luSeed);
    }

    s32  CalculateNumberOfCarsInFlyby();                                        // 0x82357F08
    void CalculateOnlineRivals();                                              // 0x823861C0

    void CalculateRivalryRating(FlybyManager::RivalRating* lpRating);          // 0x8236DAE0
    void CalculateNewOrOngoingRivalryRating(FlybyManager::RivalRating* lpRating); // 0x82358028
    void CalculateMarkedManRivalryRating(FlybyManager::RivalRating* lpRating);  // 0x823642F0
    void CalculatePointsLeaderRating(FlybyManager::RivalRating* lpRating);      // 0x82358148

    const CgsNetwork::PlayerName* GetRivalName(BrnNetwork::NetworkPlayerID lPlayerID);

    s32  GetOngoingStat(FlybyManager::RivalRating* lpRating,
                        EOngoingRivalryCompare leStatType, EMessageStyle leStyle); // 0x823582A0
    s32  GetNewRivalryStat(FlybyManager::RivalRating* lpRating,
                           ENewRivalryCompare leStatType, EMessageStyle leStyle);  // 0x82358418
    s32  GetNoRivalryStat(FlybyManager::RivalRating* lpRating,
                          ENoRivalryCompare leStatType);                           // 0x82364488

    s32  CountAvailableOngoingRivalryMessages(EOngoingRivalryCompare leStatType,
                                              EMessageStyle leStyle);               // 0x82358570
    s32  CountAvailableNewRivalryMessages(ENewRivalryCompare leStatType,
                                          EMessageStyle leStyle);                   // 0x82358648

    BrnNetwork::NetworkPlayerID GetLocalPlayerNetworkID();

    // Reference  /. GetPlayerStats is inlined at every console site (search, then the
    // record's leading NetworkPlayerStats); GetLiveRevengeRelationship also has an out-of-line copy.
    const BrnNetwork::NetworkPlayerStats*      GetPlayerStats(BrnNetwork::NetworkPlayerID lPlayerID)
    {
        return &mPlayerInputInterface.GetPlayerStatusDataByPlayerID(lPlayerID)->mPlayerStats;
    }
    const BrnNetwork::LiveRevengeRelationship* GetLiveRevengeRelationship(BrnNetwork::NetworkPlayerID lPlayerID) const;

    void CalculateDisconnectRating(FlybyManager::RivalRating* lpRating);
    void CalculateTeamRating(FlybyManager::RivalRating* lpRating);
    void AddNoRivalryFlybyMessage(FlybyManager::RivalRating* lpRating);
    void AddNewRivalryMessage(FlybyManager::RivalRating* lpRating);
    void AddOngoingRivalryMessage(FlybyManager::RivalRating* lpRating);

private:
    BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface mPlayerInputInterface;       // console +0x2A0
    CgsContainers::FastBitArray<5>                              mAvailableNoRivalryMessages; // console +0xC90
};

// ===========================================================================
// Cross-TU boundary shims.
//
// The functions below stand for console code reached by the twelve reconstructed functions whose
// real home is not reconstructed: an unnamed ScoringSystem player lookup by network id and the
// inlined FlybyData::SetMessageStyle. DECLARED here (no body), reached BY NAME.
// ===========================================================================
// The X360 cross-references a network player to its active-race-car index via either the module's
// or the scoring system's player table (sub_8231DD88). Two overloads keep the call sites typed.
s32 ModulePlayerActiveRaceCarIndex(GameStateModule* lpModule, BrnNetwork::NetworkPlayerID lPlayerID);
s32 ModulePlayerActiveRaceCarIndex(ScoringSystem* lpScoringSystem, BrnNetwork::NetworkPlayerID lPlayerID);

void FlybyData_TagLastCarStyle(GameStateModuleIO::FlybyData* lpFlybyData, s32 liStyle);

// The pre-race rivalry message tables: per stat and message style, up to KI_MAX_MESSAGES string-id
// pairs, unused slots null. CountAvailable* count a row's leading non-null plural ids; the Add*
// builders pick one entry at random. Defined in the .cpp with the image's contents.
const s32 KI_MAX_MESSAGES = 5;
extern const CombinedStringID KA_ONGOING_RIVALRY_MESSAGES
    [OnlineFlybyManager::E_ONGOING_RIVALRY_COUNT][OnlineFlybyManager::E_MESSAGE_STYLE_COUNT][KI_MAX_MESSAGES];
extern const CombinedStringID KA_NEW_RIVALRY_MESSAGES
    [OnlineFlybyManager::E_NEW_RIVALRY_COUNT][OnlineFlybyManager::E_MESSAGE_STYLE_COUNT][KI_MAX_MESSAGES];
extern const CombinedStringID KA_NO_RIVALRY_MESSAGES[OnlineFlybyManager::E_NO_RIVALRY_COUNT];

// Post-increment over the three stat enums, asserting the walk never passes the COUNT sentinel.
inline OnlineFlybyManager::ENoRivalryCompare operator++(OnlineFlybyManager::ENoRivalryCompare& leEnumIndex, int)
{
    const OnlineFlybyManager::ENoRivalryCompare leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<OnlineFlybyManager::ENoRivalryCompare>(static_cast<s32>(leEnumIndex) + 1);
    CGS_ASSERT(leEnumIndex <= OnlineFlybyManager::E_NO_RIVALRY_COUNT,
               "leEnumIndex <= OnlineFlybyManager::E_NO_RIVALRY_COUNT");
    return leOldEnumIndex;
}

inline OnlineFlybyManager::EOngoingRivalryCompare operator++(OnlineFlybyManager::EOngoingRivalryCompare& leEnumIndex, int)
{
    const OnlineFlybyManager::EOngoingRivalryCompare leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<OnlineFlybyManager::EOngoingRivalryCompare>(static_cast<s32>(leEnumIndex) + 1);
    CGS_ASSERT(leEnumIndex <= OnlineFlybyManager::E_ONGOING_RIVALRY_COUNT,
               "leEnumIndex <= OnlineFlybyManager::E_ONGOING_RIVALRY_COUNT");
    return leOldEnumIndex;
}

inline OnlineFlybyManager::ENewRivalryCompare operator++(OnlineFlybyManager::ENewRivalryCompare& leEnumIndex, int)
{
    const OnlineFlybyManager::ENewRivalryCompare leOldEnumIndex = leEnumIndex;
    leEnumIndex = static_cast<OnlineFlybyManager::ENewRivalryCompare>(static_cast<s32>(leEnumIndex) + 1);
    CGS_ASSERT(leEnumIndex <= OnlineFlybyManager::E_NEW_RIVALRY_COUNT,
               "leEnumIndex <= OnlineFlybyManager::E_NEW_RIVALRY_COUNT");
    return leOldEnumIndex;
}
}
