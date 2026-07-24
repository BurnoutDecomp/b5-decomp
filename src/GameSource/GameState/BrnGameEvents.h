#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // EPlayerTeam
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex (StartNetworkRoundEvent)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h" // CgsContainers::FastBitArray<60> (LastSecondChallengeSuccess)
#include "GameSource/GameState/BrnCgsPlayerName.h"           // CgsNetwork::PlayerName (BuddyRemovedEvent)
#include "SharedClasses/StreetData/BrnStreetData.h"          // BrnStreetData::ChallengeIndex (road-rules events)
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h" // BrnStreetData::ChallengeHighScoreEntry (PB-recv event)

// Owning header for the BrnGameState::GameStateModuleIO GameEvent<> family slices reconstructed
// by the GameMode/ModeManager leaf batch. Minimal slices: only members the reconstructed bodies
// touch. GameEvent<T> is an empty template spine (the real build adds only a static type tag);
// its real Event base + the per-event mseType definitions land with the full BrnGameEvents TU.

namespace BrnGameState
{
namespace GameStateModuleIO
{
// Max players in a network game (== BrnWorld::KI_MAX_ACTIVE_RACE_CARS on this build).
const s32 KI_MAX_RACE_CARS = 8;

// EGameEventType discriminant. Only the slots this batch instantiates are listed; the unconfirmed
// values are placeholders used purely as template tags.
enum EGameEventType
{
    E_EVENT_STREAMING_COMPLETE      = 9,   // DWARF BrnGameEvents.h:19 (full contiguous EGameEventType)
    E_EVENT_CHANGE_NETWORK_CAR      = 7,
    E_EVENT_ONLINE_PLAYER_ADDED     = 127,
    E_EVENT_ONLINE_PLAYER_FINALISED = 128,
    E_EVENT_ONLINE_PLAYER_REMOVED   = 129,   // value unconfirmed (template tag only)
    E_EVENT_START_NETWORK_GAME      = 17,    // DWARF BrnGameEvents.h (full contiguous EGameEventType)
    E_EVENT_START_NETWORK_ROUND     = 18,    // DWARF BrnGameEvents.h
    E_EVENT_REMOTE_PLAYER_DISCONNECTED = 131, // value unconfirmed (template tag only)
    E_EVENT_RECORD_PROP_HIT         = 111,   // DWARF BrnGameEvents.h:121
    E_EVENT_OVERHEAD_SIGN_HIT       = 118,   // DWARF BrnGameEvents.h:76
    // Freeburn-challenge events (PS3-DWARF values; used as template tags -- the X360
    // discriminants ChallengeManager::ProcessEvent actually switches on are the raw
    // jump-table case values in that body, which drift from these).
    // NOTE: the X360 values of these two are UNKNOWN but are provably NOT 165/166 -- the
    // ChallengeManager::ProcessEvent jump table attests X360 165 == action-success and
    // 166 == challenge-reset (see the X360-attested block below; duplicate enumerator
    // values are intentional). Template tags only; do not switch on these two by name.
    E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE = 165, // DWARF BrnGameEvents.h:175 (value unconfirmed on X360)
    E_EVENT_FREEBURN_CHALLENGE_SUCCESS        = 166, // DWARF BrnGameEvents.h:176 (value unconfirmed on X360)
    // ---- X360-ATTESTED discriminants (ChallengeManager::ProcessEvent 0x8233D6A8 jump
    // table: r11 = type - 54, 120 slots; the assert strings name each case's event).
    // PS3-DWARF values drift NON-uniformly (noted per enumerator) -- keep the X360 values.
    E_EVENT_POWER_PARK_RESULT                 = 54,  // X360 (PS3 DWARF 55)
    E_EVENT_BOOST_TIME_COMPLETE               = 55,  // X360 (PS3 DWARF 56; "lpBoostTimeComplete")
    E_EVENT_NEAR_MISS                         = 65,  // X360 == PS3 ("lpNearMissEvent")
    E_EVENT_NEAR_MISS_CHAIN_COMPLETED         = 66,  // X360 (PS3 67; "lpNearMissCompleteEvent")
    E_EVENT_DRIFTING                          = 67,  // X360 (PS3 68; "lpDriftEvent")
    E_EVENT_ONCOMING                          = 70,  // X360 (PS3 71)
    E_EVENT_ONCOMING_COMPLETED                = 71,  // X360 (PS3 72)
    E_EVENT_COMPLETED_STUNT                   = 119, // X360 == PS3 ("lpCompletedStuntEvent")
    E_EVENT_INPROGRESS_STUNT                  = 120, // X360 == PS3 ("lpInProgressStuntEvent")
    E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS = 165, // X360 (PS3 160; "lpActionSuccessEvent")
    E_EVENT_FREEBURN_CHALLENGE_RESET          = 166, // X360 (PS3 161; "lpResetEvent" -- reset ONE action slot)
    // X360-only discriminant: same "lpResetEvent" payload, but resets ALL action slots
    // (loop ResetActionData). No PS3 enumerator exists at this position; the NAME is
    // reconstructed from the case body (FLAG).
    E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS = 167, // X360-attested value; FLAGGED name
    E_EVENT_ACTIVE_FREEBURN_CHALLENGE         = 173, // X360 (PS3 167; "lpActiveChallengeEvent")
    // Road-rules events (StreetManager keystone, wave B). PS3-DWARF-derived tags;
    // values unconfirmed on X360 (template tags only -- the retail discriminants are
    // the ProcessGameEvents jump-table cases, owned by that dispatcher TU).
    E_EVENT_BUDDY_REMOVED                    = 190,  // value unconfirmed (template tag only)
    E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST     = 191,  // value unconfirmed (template tag only)
    E_EVENT_ONLINE_ROAD_RULES_PB_RECV        = 192,  // value unconfirmed (template tag only)
    E_EVENT_ONLINE_ROAD_RULES_UPLOADED       = 193,  // value unconfirmed (template tag only)
    E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED     = 194,  // value unconfirmed (template tag only)
    E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO   = 195,  // value unconfirmed (template tag only)
};

template <EGameEventType T>
struct GameEvent { };

// X360 element of EventQueue<HitOverheadSignEvent,100> (DWARF BrnGameEvents.h:429). Single byte.
struct HitOverheadSignEvent : public GameEvent<E_EVENT_OVERHEAD_SIGN_HIT>
{
    u8 muRaceCarId;   // 0x00
};

// X360 element of EventQueue<RecordPropHitEvent,50> (DWARF BrnGameEvents.h:413). 16-byte aligned
// via the leading Vector3.
// DWARF BrnGameEvents.h:744 -- the "a module finished streaming" event
// (E_EVENT_STREAMING_COMPLETE == 9). WorldEntityModule::UpdateStream posts it with
// meModule == E_MODULE_WORLD_GRAPHICS when a GameAction asked to be told the world
// stream settled (the X360 16-byte payload {2, 0}).
struct StreamingCompleteEvent : public GameEvent<E_EVENT_STREAMING_COMPLETE>
{
    // BrnGameEvents.h:757 nested enum; value 2 attested by the X360 UpdateStream store.
    enum EModule
    {
        E_MODULE_WORLD_GRAPHICS = 2,
    };

    EModule meModule;   // :757
    CgsID   mUserId;    // :758
};

struct RecordPropHitEvent : public GameEvent<E_EVENT_RECORD_PROP_HIT>
{
    Vector3 mPosition;   // 0x00 (rw::math::vpu, 16-byte SIMD)
    u16     muZoneId;    // 0x10
    u16     muPropId;    // 0x12
    bool    mbHitBefore; // 0x14
};

// FLAG (minimal home): FinishedSyncingPlayersEvent -- a BrnGameState::GameStateModuleIO network-sync
// notification event queued by BrnNetwork::StateManager::UpdateSyncTime via
// CgsModule::VariableEventQueue<1536,16>::AddEvent<FinishedSyncingPlayersEvent> @ 0x82566168, which
// forwards liSize == sizeof(EventT) == 1 (li r6,1 @ 0x82566204). Neither the member layout nor the
// EGameEventType discriminant is attested by any decompiled caller or by DWARF (the sole caller
// UpdateSyncTime is not yet decompiled), so this is homed at the asm-attested size only: a 1-byte
// opaque marker. Grow to the real GameEvent<E_EVENT_...> shape once the caller is recovered.
struct FinishedSyncingPlayersEvent
{
    u8 muPad0;   // 0x00 -- asm-attested size (1) only; real field(s)/discriminant unknown
};

// X360 0x823A78F0. mNetworkPlayerID at offset 0.
struct ChangeNetworkCarEvent : public GameEvent<E_EVENT_CHANGE_NETWORK_CAR>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A77D0. mNetworkPlayerID at offset 0x10 (after two CgsID).
struct OnlinePlayerAddedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_ADDED>
{
    CgsID                       mModelID;          // 0x00
    CgsID                       mWheelID;          // 0x08
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;  // 0x10

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A7830. mNetworkPlayerID at offset 0.
struct OnlinePlayerFinalisedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_FINALISED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A7890. mNetworkPlayerID at offset 0.
struct OnlinePlayerRemovedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_REMOVED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A7770. mNetworkPlayerID at offset 0.
struct RemotePlayerDisconnectedEvent : public GameEvent<E_EVENT_REMOTE_PLAYER_DISCONNECTED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x82542068 (Clear) / 0x825420C8 (SetPlayerData). Full 256-byte X360 layout (grown from the
// former minimal slice): the three X360 facts that NetworkRoundManager::NetworkGameStarted bakes --
// miNumRounds @ offset 8, mbIsStartingGameAfterPlayerJoin @ offset 248, sizeof == 256 (memcpy) --
// all land EXACTLY with this member set/order. mafPlayerData[8] is RETAINED (it pushes
// mbIsStartingGameAfterPlayerJoin to 248; absent from the PS3 DWARF, but the X360 offset proves it
// real). meBoostType spelled s32 (BrnNetwork::EBoostType has no committed home; same 4B storage).
struct StartNetworkGameEvent : public GameEvent<E_EVENT_START_NETWORK_GAME>
{
    s32                         miNumRaceCars;                            // 0x00
    EGameModeType               meGameMode;                               // 0x04
    s32                         miNumRounds;                              // 0x08
    u32                         muRandomSeedForGame;                      // 0x0C
    bool                        mbRefreshOnly;                            // 0x10
    CgsID                       maCarIds[KI_MAX_RACE_CARS];               // 0x18
    u16                         mau16CarColourIndex[KI_MAX_RACE_CARS];
    u16                         mau16CarPaintFinishIndex[KI_MAX_RACE_CARS];
    EPlayerTeam                 maePlayerTeam[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID maNetworkPlayerID[KI_MAX_RACE_CARS];
    f32                         mafPlayerData[KI_MAX_RACE_CARS];          // 0xB8
    bool                        mabPlayerHasFever[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID mLocalNetworkPlayerID;                    // 0xE0 (224)
    s32                         miHostGridPosition;                       // 0xE4 (228)
    s32                         miNumRunnerCrashes;                       // 0xE8 (232)
    s32                         meBoostType;                              // 0xEC (236) BrnNetwork::EBoostType (s32)
    f32                         mfTimeLimit;                              // 0xF0 (240)
    bool                        mbRedTeamHaveInfiniteBoost;               // 0xF4 (244)
    bool                        mbIsTrafficOn;                            // 0xF5 (245)
    bool                        mbIsTrafficCheckingOn;                    // 0xF6 (246)
    bool                        mbIsRanked;                               // 0xF7 (247)
    bool                        mbIsStartingGameAfterPlayerJoin;          // 0xF8 (248)
    bool                        mbIsStartingFreeburnLobbyAfterOnlineEvent; // 0xF9 (249)
    bool                        mbForceStartFreeburnLobby;                // 0xFA (250)

    void Clear();
    void SetPlayerData(s32                         liPlayerIndex,
                       BrnNetwork::NetworkPlayerID liNetworkPlayerID,
                       CgsID                       lCarId,
                       f32                         lfPlayerData,
                       u16                         lu16CarColourIndex,
                       u16                         lu16CarPaintFinishIndex,
                       EPlayerTeam                 lePlayerTeam,
                       bool                        lbPlayerHasFever,
                       bool                        lbUpdateLocalNetworkPlayerID);
};

// X360 element copied as 10 dwords (40 bytes) by NetworkRoundManager::NetworkRoundStarted
// (0x823589E8). sizeof == 40: 16*LandmarkIndex(2) + u32 + s32. mLightTriggerID spelled u32
// (LightTriggerId == u32; matches the BrnGameStateSharedIO.h precedent).
struct StartNetworkRoundEvent : public GameEvent<E_EVENT_START_NETWORK_ROUND>
{
    BrnGameState::LandmarkIndex maLandmarks[16];      // 0x00, 16*2 = 32 bytes
    u32                         mLightTriggerID;      // 0x20 (LightTriggerId == u32)
    s32                         miNumLandmarksInRound; // 0x24
}; // sizeof == 40

// ===== Freeburn-challenge events (ChallengeManager keystone) =====
// (LastSecondChallengeSuccess == FastBitArray<60> lives at its DWARF home,
//  BrnGameStateSharedIO.h:313, pulled in by the include above.)

// DWARF BrnGameEvents.h:2893 (member set + order); X360 field offsets attested by
// ChallengeManager::HandleSuccessUpdateEvent 0x8233CDE0 (mask u64 @0x00, arci @0x08,
// frame @0x0C -- compared against the manager's miLastChallengeResetFrame -- action @0x10).
struct FburnChallengeSuccessUpdateEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE>
{
    LastSecondChallengeSuccess mChallengeSuccessUpdate; // 0x00 (FastBitArray<60>, 8 bytes)
    EActiveRaceCarIndex        meActiveRaceCarIndex;    // 0x08
    s32                        miChallengeUpdateFrame;  // 0x0C
    s32                        miActionIndex;           // 0x10
};

// DWARF BrnGameEvents.h:2910 (member set + order). X360 DRIFT: HandleChallengeSuccessEvent
// 0x82316AE0 reads a frame number @0x0C (gated against miLastChallengeResetFrame) and the
// ARCI @0x10 -- the X360 build inserted miChallengeUpdateFrame before meActiveRaceCarIndex
// (absent from the PS3 DWARF member list).
struct FburnChallengeSuccessEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_SUCCESS>
{
    f32                 mafActionScores[2];         // 0x00
    bool                mabSuccessfulActions[2];    // 0x08
    bool                mabAccumulationThisFrame[2];// 0x0A
    s32                 miChallengeUpdateFrame;     // 0x0C (X360-only member; asm-attested)
    EActiveRaceCarIndex meActiveRaceCarIndex;       // 0x10
};

// ===== Road-rules events (StreetManager keystone, wave B) =====
// ADDITIVE GROW. Layouts are DWARF BrnGameEvents.h:1054/:2063/:2424/:2440/:2454/:2467;
// field offsets X360-proven by the StreetManager handler bodies (the GameEvent<> base is
// empty, so payloads start at +0 -- e.g. ProcessNetworkHighScoreEvent @ 0x82349F10 copies
// the 56-byte score from event+0 and reads the challenge index at +60 / friend flag at +64;
// ProcessConnectedOnlineEvent @ 0x8234A148 reads the reset time at +0).
// The E_EVENT_* tag values below are placeholders (template tags only, X360 discriminants
// owned by the GameStateModule::ProcessGameEvents dispatcher TU) on the committed
// unconfirmed-value precedent above.

// Buddy removed from the friends list (DWARF :1054): just the removed buddy's name.
// ProcessBuddyRemoved @ 0x8234A5A8 hands &mRemovedBuddyName straight to
// ClearAllChallengeDataForBuddy (CgsNetwork::PlayerName semantics, 16 bytes).
struct BuddyRemovedEvent : public GameEvent<E_EVENT_BUDDY_REMOVED>
{
    CgsNetwork::PlayerName mRemovedBuddyName;   // 0x00 (16B)
};

// GUI requests the score breakdown for one road (DWARF :2063).
struct RoadRulesScoreRequestEvent : public GameEvent<E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST>
{
    BrnStreetData::ChallengeIndex mRoadChallengeIndex;   // 0x00 (Road::ChallengeIndex == int32)
};

// A downloaded online personal-best record (DWARF :2424).
struct OnlineRoadRulesPersonalBestRecvEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_PB_RECV>
{
    BrnStreetData::ChallengeHighScoreEntry mPersonalBestScore;          // 0x00 (56B)
    BrnNetwork::NetworkPlayerID            mPersonalBestPlayerID;       // 0x38 (RoadRulesRecvData::NetworkPlayerID == s32)
    BrnStreetData::ChallengeIndex          mPersonalBestChallengeIndex; // 0x3C
    bool                                   mbWasPBByFriend;             // 0x40
};

// Road-rules score upload completed for a challenge-index range (DWARF :2440).
struct OnlineRoadRulesUploadedEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_UPLOADED>
{
    BrnStreetData::ChallengeIndex mStartUploadIndex;   // 0x00
    BrnStreetData::ChallengeIndex mEndUploadIndex;     // 0x04
};

// Road-rules download completed (DWARF :2454).
struct OnlineRoadRulesDownloadedEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED>
{
    u32 muTimestampOfDownload;   // 0x00
};

// Road-rules server connect info (DWARF :2467).
struct OnlineRoadRulesConnectInfoEvent : public GameEvent<E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO>
{
    u32 muLastRoadRulesResetTime;   // 0x00
};

// ===== ChallengeManager::ProcessEvent payloads (ChallengeManager keystone, wave C) =====
// ADDITIVE GROW. The sole reconstructed consumer is ChallengeManager::ProcessEvent
// (X360 0x8233D6A8); every field offset below is attested by that body's raw loads (the
// GameEvent<> base is empty, so payloads start at +0x00). DWARF BrnGameEvents.h lines
// noted per struct; the two stunt events DRIFT heavily from the lean PS3 records and are
// modelled X360-authoritatively (flagged members).

// Power-park scored (DWARF :2812). X360 case 54: meOutcome (lwz +0) == 1 (E_PPO_SUCCESS)
// gates; miOtherPlayersInvolved (lwz +8) >= 2 selects PLAYER_POWER_PARKING(11) over
// TRAFFIC_POWER_PARKING(12); miOverallRating (lwz +4, fcfid -> f32) is the score.
// meOutcome is BrnWorld::EPowerParkOutcome stored as s32 (the enum's provisional home is
// BrnGameActions.h; s32 storage keeps this header decoupled -- StartNetworkGameEvent
// precedent).
struct PowerParkResultEvent : public GameEvent<E_EVENT_POWER_PARK_RESULT>
{
    s32 meOutcome;               // 0x00 (:2814, BrnWorld::EPowerParkOutcome; s32 storage)
    s32 miOverallRating;         // 0x04 (:2815)
    s32 miOtherPlayersInvolved;  // 0x08 (:2816)
};

// Boost ran out / completed (DWARF :2826). X360 case 55: the f32 at +0 feeds
// E_FREEBURN_SKILL_BOOST_TIME(14).
struct BoostTimeCompleteEvent : public GameEvent<E_EVENT_BOOST_TIME_COMPLETE>
{
    f32 mfTimeSpentBoosting;   // 0x00 (:2828)
};

// Near miss scored (DWARF :1489). X360 case 65: miCount (lwz +0, fcfid -> f32) feeds
// E_FREEBURN_SKILL_NEAR_MISS(4). meNearMissType is BrnWorld::ENearMissType stored as s32
// (its committed home is BrnNearMissManager.h; not read by this consumer).
struct NearMissEvent : public GameEvent<E_EVENT_NEAR_MISS>
{
    s32 miCount;         // 0x00 (:1490)
    s32 meNearMissType;  // 0x04 (:1491, BrnWorld::ENearMissType; s32 storage)
};

// Near-miss chain completed (DWARF :1516). X360 case 66: miCount (lwz +0, fcfid) feeds
// E_FREEBURN_SKILL_NEAR_MISS(4) -- same tail as case 65.
struct NearMissChainCompleteEvent : public GameEvent<E_EVENT_NEAR_MISS_CHAIN_COMPLETED>
{
    s32  miCount;                 // 0x00 (:1517)
    bool mbCompletedSuccessfully; // 0x04 (:1518; not read by ProcessEvent)
};

// Drift scored (DWARF :1586). X360 case 67: the f32 at +0 feeds E_FREEBURN_SKILL_DRIFT(3).
struct DriftingEvent : public GameEvent<E_EVENT_DRIFTING>
{
    f32 mfDistance;   // 0x00 (:1587)
};

// Oncoming (driving into oncoming traffic) in progress / completed (DWARF :1975/:1988).
// X360 cases 70/71: the f32 at +0 feeds E_FREEBURN_SKILL_ONCOMING(0).
struct OncomingEvent : public GameEvent<E_EVENT_ONCOMING>
{
    f32 mfDistance;   // 0x00 (:1976)
};
struct OncomingCompletedEvent : public GameEvent<E_EVENT_ONCOMING_COMPLETED>
{
    f32 mfDistance;   // 0x00 (:1989)
};

// A freeburn-challenge action succeeded (network echo) (DWARF :2507). X360 case 165:
// mChallengeID (64-bit compare vs mpCurrentChallenge's id), miActionIndex (bound-asserted
// "lpActionSuccessEvent->miActionIndex < mpCurrentChallenge->GetNumActions()").
struct FreeburnChallengeActionSuccessEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS>
{
    CgsID mChallengeID;  // 0x00 (:2509)
    s32   miActionIndex; // 0x08 (:2510)
};

// A freeburn challenge (or one of its action slots) was reset (DWARF :2522). SHARED by the
// X360 cases 166 (reset the one slot miActionIndex) and 167 (reset slots 0..miActionIndex)
// -- both assert "lpResetEvent" and 64-bit-compare mChallengeID vs the current challenge's
// (Hex-Rays renders that cmpld as a nonsense self-compare of the entry's id halves; the raw
// asm is `ld +0(event); ld +0xC0(challenge); cmpld`).
struct FreeburnChallengeResetEvent : public GameEvent<E_EVENT_FREEBURN_CHALLENGE_RESET>
{
    CgsID mChallengeID;  // 0x00 (:2524)
    s32   miActionIndex; // 0x08 (:2525)
};

// The active-challenge announcement received on join (DWARF :2927). X360 case 173 reads
// count @0x28, walks the ARCI block from +0x00 (bound asserts spell
// "lpActiveChallengeEvent->maePlayersInChallengeARCI[liIndex] ...") and resolves the
// challenge from the CgsID @0x20. NOTE: this event KEEPS the PS3 member order -- the
// matching ActiveFburnChallengeAction (BrnGameActions.h) is the one whose X360 order drifts.
struct ActiveFburnChallengeEvent : public GameEvent<E_EVENT_ACTIVE_FREEBURN_CHALLENGE>
{
    static const s32 KI_MAX_NETWORK_PLAYERS = 7;

    EActiveRaceCarIndex maePlayersInChallengeARCI[KI_MAX_NETWORK_PLAYERS]; // 0x00..0x1B (:2929)
    CgsID               mChallengeID;             // 0x20 (:2930; 8-aligned)
    s32                 miNumPlayersInChallenge;  // 0x28 (:2931)
}; // sizeof == 0x30

// ---- X360 stunt events (heavy X360-vs-DWARF drift; modelled X360-authoritatively) ----
// The PS3 DWARF records CompletedStuntEvent (:497) / InProgressStuntEvent (:524) as lean
// scalar structs. The X360 build's records are LARGER: they carry a per-stunt-run-slot
// score/flag block (the 19 X360-only stunt-run skills) and, on the in-progress event, the
// same convoy block OnStuntElementCompleteAction (BrnGameActions.h) carries at identical
// offsets (member ids @0x44, count @0x64). Every named field below is offset-attested by
// a ProcessEvent raw load; DWARF names are applied where the consuming skill id proves the
// semantic (noted); X360-only members are FLAGGED. maReserved* gaps are DWARF-silent.
//
// The stunt-run block: both events read f32 @ (4 * K) and (completed only) the byte
// @ (0x44 + K - 1), where K == the type-22/23 action's GetTargetValue(1) (the action's
// 1-BASED stunt-run slot; ChallengeListEntryAction+0x38). So mafStuntRunScores[K-1] sits
// at +0x04 and mabStuntRunScored[K-1] at +0x44. The [12] extents are the largest that fit
// the attested neighbours (scores < +0x34 tail region, flags < +0x50) -- FLAGGED.
struct CompletedStuntEvent : public GameEvent<E_EVENT_COMPLETED_STUNT>
{
    u32  muStuntActionComplete;        // 0x00 (:499; completed-stunt-kind bit mask: 0x1 barrel
                                       //       roll, 0x2 flatspin, 0x10 landing, 0x40/0x80 drift,
                                       //       0x100 air, 0x200 air distance, 0x400 stunt-run,
                                       //       0x800 skill-37 count)
    f32  mafStuntRunScores[12];        // 0x04..0x33 FLAG: X360-only, 1-based slot K reads [K-1]
    u8   maReserved0x34[0x10];         // 0x34..0x43 (DWARF-silent gap; true members unrecovered)
    bool mabStuntRunScored[12];        // 0x44..0x4F FLAG: X360-only ("slot K scored" gate)
    s32  miCompletedBarrelRolls;       // 0x50 (:511; lwz+fcfid, feeds BARREL_ROLL(9)/(10))
    s32  miCompletedSkill37Count;      // 0x54 FLAG: X360-only count (lwz+fcfid, feeds drifted skill 37)
    u8   maReserved0x58[4];            // 0x58..0x5B
    f32  mfCompletedAirSpinAngle;      // 0x5C (:502; radians -- consumer scales by 57.29578,
                                       //       feeds FLATSPIN(1)/FLATSPIN_REVERSE(2))
    u8   maReserved0x60[8];            // 0x60..0x67
    f32  mfCompletedDriftDistance;     // 0x68 (feeds DRIFT(3); drift skill is distance-valued per
                                       //       DriftingEvent -- DWARF also lists a DriftTime, FLAG)
    f32  mfCompletedAirTime;           // 0x6C (:506; feeds AIR(15))
    f32  mfCompletedAirDistance;       // 0x70 (:508; feeds AIR_DISTANCE(16))
    u8   maReserved0x74[0xC];          // 0x74..0x7F
    bool mbSuccessfulLanding;          // 0x80 (:507; feeds SUCCESSFUL_LANDING(5)/(6))
    bool mbInReverse;                  // 0x81 (:509; selects the _REVERSE twin skill)
    bool mbStuntRunEnded;              // 0x82 FLAG: X360-only (latches drifted skill 19 to 0.0)
};

struct InProgressStuntEvent : public GameEvent<E_EVENT_INPROGRESS_STUNT>
{
    u32  muStuntActionInProgress;      // 0x00 (:526; in-progress bit mask: 0x1 barrel roll,
                                       //       0x2 flatspin, 0x20 air, 0x40 air distance,
                                       //       0x80 convoy/stunt-run)
    f32  mafStuntRunScores[12];        // 0x04..0x33 FLAG: X360-only (same 1-based slot block)
    u8   maReserved0x34[0x10];         // 0x34..0x43 (mirrors OnStuntElementCompleteAction's
                                       //       maConvoyLegDistances region; unread here -- pad)
    s32  maConvoyMemberARCIs[8];       // 0x44..0x63 FLAG: X360-only (walked player-by-player;
                                       //       same offsets as OnStuntElementCompleteAction's
                                       //       maConvoyMemberIds)
    s32  miConvoyMemberCount;          // 0x64 FLAG: X360-only (the `count > 0` loop bound)
    f32  mfInProgressBarrelRollAngle;  // 0x68 (:528; radians -- consumer converts to whole rolls,
                                       //       feeds BARREL_ROLL(9)/(10))
    f32  mfInProgressAirSpinAngle;     // 0x6C (:529; radians, feeds FLATSPIN(1)/(2))
    u8   maReserved0x70[0xC];          // 0x70..0x7B
    f32  mfTimeInAir;                  // 0x7C (:533; feeds AIR(15))
    f32  mfDistanceInAir;              // 0x80 (:534; feeds AIR_DISTANCE(16))
    u8   maReserved0x84[0xC];          // 0x84..0x8F
    bool mbInReverse;                  // 0x90 (:535; selects the _REVERSE twin skill)
};

// Pin the attested offsets (both structs are pointer-free -> absolute on the x64 gate).
static_assert(offsetof(CompletedStuntEvent, mabStuntRunScored) == 0x44, "CompletedStuntEvent stunt-run flags at +0x44");
static_assert(offsetof(CompletedStuntEvent, miCompletedBarrelRolls) == 0x50, "CompletedStuntEvent barrel rolls at +0x50");
static_assert(offsetof(CompletedStuntEvent, mfCompletedAirSpinAngle) == 0x5C, "CompletedStuntEvent air-spin angle at +0x5C");
static_assert(offsetof(CompletedStuntEvent, mbSuccessfulLanding) == 0x80, "CompletedStuntEvent landing flag at +0x80");
static_assert(offsetof(InProgressStuntEvent, maConvoyMemberARCIs) == 0x44, "InProgressStuntEvent convoy ids at +0x44");
static_assert(offsetof(InProgressStuntEvent, miConvoyMemberCount) == 0x64, "InProgressStuntEvent convoy count at +0x64");
static_assert(offsetof(InProgressStuntEvent, mfTimeInAir) == 0x7C, "InProgressStuntEvent air time at +0x7C");
static_assert(offsetof(InProgressStuntEvent, mbInReverse) == 0x90, "InProgressStuntEvent reverse flag at +0x90");
static_assert(offsetof(ActiveFburnChallengeEvent, mChallengeID) == 0x20, "ActiveFburnChallengeEvent id at +0x20");
static_assert(offsetof(ActiveFburnChallengeEvent, miNumPlayersInChallenge) == 0x28, "ActiveFburnChallengeEvent count at +0x28");
}
}
