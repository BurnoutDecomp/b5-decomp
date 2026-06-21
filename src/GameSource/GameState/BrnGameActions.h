#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector3, CgsID, EntityId
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // EPlayerScoringIndex, EPlayerTeam
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // BrnGameState::GameModeParams (PrepareForModeAction payload)
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<T, N> (SetUpAllDriveThrusAction::maDriveThrus)
#include "GameSource/GameState/Offences/BrnDriveThruManager.h"  // BrnTrigger::GenericRegion::Type (DriveThruInfo::meType)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"     // CgsSystem::Time (OnlineGameResults::mSecondsInEvent / maRoundTimes)
#include "GameSource/GameState/BrnGameStateTypes.h"           // BrnGameState::StuntElementType (WorldStuntAction / OnStuntElementCompleteAction)

// Owning header for the BrnGameState::GameStateModuleIO GameAction<> family slices reconstructed
// by the GameMode/ModeManager leaf batch. Each struct is a minimal slice: only the members the
// reconstructed body touches are declared. The GameAction<T> base is modelled as an empty
// template spine (the real build adds only a static type tag, no instance data/vtable); its real
// Event base + the per-action mseType definitions land with the full BrnGameActions TU.

// Provisional enum home for the TrophyUnlockAction element's unlock-type field. No committed
// home exists for BrnProgression::TrophyUnlockData::UnlockType; declare just the type tag (the
// full 35-value enum + the TrophyUnlockData struct are their own TU,
// SharedClasses/Progression/BrnTrophyUnlockData.h). Append/Erase never read this member -- it
// only needs the element type to be a complete 16-byte type so the memberwise copy is the right
// width. (DWARF SharedClasses/Progression/BrnTrophyUnlockData.h:48, E_UNLOCKTYPE_COUNT=35.)
namespace BrnProgression
{
struct TrophyUnlockData
{
    enum UnlockType
    {
        E_UNLOCKTYPE_NONE  = 0,
        E_UNLOCKTYPE_COUNT = 35,
    };
};
}

// Provisional enum home for PowerParkResultAction::meOutcome. The committed owner is
// BrnWorld::EPowerParkOutcome, whose real home (DWARF
// World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h:11) has not been
// reconstructed yet -- there is no PowerParking TU in b5-decomp. The full enum is mirrored here
// (X360 DealWithPowerPark @0x82321530 only tests meOutcome == E_PPO_SUCCESS(1), but the complete
// 4-value set is DWARF-confirmed) so PowerParkResultAction is a complete type. Re-home / replace
// this with an #include of BrnPowerParkingManager.h when that TU lands; do not let two definitions
// coexist (this is provisional only).
namespace BrnWorld
{
enum EPowerParkOutcome
{
    E_PPO_TO_BE_DETERMINED = 0,
    E_PPO_SUCCESS          = 1,
    E_PPO_FAILURE          = 2,
    E_PPO_COUNT            = 3,
};
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
// EGameActionType discriminant. Only the slots this batch instantiates are listed (the full enum
// is its own TU). The two unconfirmed values are placeholders used purely as template tags.
enum EGameActionType
{
    E_ACTION_RESET_PLAYER_CAR           = 0,
    E_ACTION_REMOTE_PLAYER_DISCONNECTED = 11,
    E_ACTION_SOUND_TRIGGER              = 210,
    E_ACTION_ONLINE_PLAYER_ADDED        = 211,   // DWARF BrnGameActions.h (was placeholder 220)
    E_ACTION_SETUP_NETWORK_CAR          = 5,     // DWARF BrnGameActions.h (was placeholder 221)
    E_ACTION_ONLINE_PLAYER_REMOVED      = 212,   // DWARF BrnGameActions.h (was placeholder 222)
    E_ACTION_ONLINE_GAME_RESULT         = 221,   // DWARF BrnGameActions.h
    E_ACTION_ONLINE_ROUND_RESULT        = 222,   // DWARF BrnGameActions.h
    E_ACTION_RANK_INFO_RESPONSE         = 173,   // DWARF BrnGameActions.h:183
    E_ACTION_TROPHY_UNLOCK              = 196,   // DWARF BrnGameActions.h:206
    E_ACTION_PREPARE_FOR_MODE           = 19,    // DWARF BrnGameActions.h:29
    E_ACTION_SET_UP_ALL_DRIVE_THRUS     = 40,    // DWARF BrnGameActions.h:40
    E_ACTION_ON_STUNT_ELEMENT_COMPLETE  = 53,    // DWARF BrnGameActions.h:63
    E_ACTION_WORLD_STUNT_PERFORMED      = 122,   // DWARF BrnGameActions.h:132
    E_ACTION_POWER_PARK_RESULT          = 139,   // DWARF BrnGameActions.h:149
};

template <EGameActionType T>
struct GameAction { };

// X360 0x822A0250 (HasToChangeLocation). Minimal slice: the two transform members it reads.
struct ResetPlayerCarAction : public GameAction<E_ACTION_RESET_PLAYER_CAR>
{
    Vector3 mPosition;
    Vector3 mDirection;

    bool HasToChangeLocation() const;
};

// X360 0x82355178 (IsEmpty).
struct SoundTriggerAction : public GameAction<E_ACTION_SOUND_TRIGGER>
{
    enum eType { E_TYPE_INVALID = 0, E_TYPE_AT_ENTITY = 1, E_TYPE_AHEAD_OF_ENTITY = 2, E_TYPE_COUNT = 3 };

    Vector3  mQueryPos;
    EntityId mEntityId;
    eType    meResultType;
    u32      muActiveTriggers;

    bool IsEmpty();
};

// X360 0x8230FE98 / 0x8230FF00.
struct RemotePlayerDisconnectedAction : public GameAction<E_ACTION_REMOTE_PLAYER_DISCONNECTED>
{
    EActiveRaceCarIndex         meActiveRaceCarIndex;
    BrnNetwork::NetworkPlayerID mPlayerID;

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);
    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lPlayerID);
};

// X360 0x82355088 (Construct). DWARF: 6 members / 0x38 bytes.
struct alignas(16) SetupNetworkCarAction : public GameAction<E_ACTION_SETUP_NETWORK_CAR>
{
    Vector3             mWorldSpacePosition;   // 0x00
    Vector3             mAt;                   // 0x10
    CgsID               mModelId;              // 0x20
    CgsID               mWheelModelId;         // 0x28
    EActiveRaceCarIndex meActiveRaceCarIndex;  // 0x30
    EPlayerScoringIndex mePlayerScoringIndex;  // 0x34

    void Construct(EPlayerScoringIndex lePlayerScoringIndex,
                   EActiveRaceCarIndex leActiveRaceCarIndex,
                   Vector3             lPos,
                   Vector3             lAt,
                   CgsID               lModelId,
                   CgsID               lWheelModelId);
};

// X360 0x823551F0 (SetPlayerScoringIndex). Layout per the Feb-2007 partial source (this X360 build).
struct OnlinePlayerAddedAction : public GameAction<E_ACTION_ONLINE_PLAYER_ADDED>
{
    CgsID               mModelID;             // 0x00
    CgsID               mWheelID;             // 0x08
    EPlayerScoringIndex mePlayerScoringIndex; // 0x10
    EPlayerTeam         meTeam;               // 0x14

    void SetPlayerScoringIndex(EPlayerScoringIndex lePlayerScoringIndex);
};

// X360 0x82355258 (SetActiveRaceCarIndex). Minimal slice: only the member the body touches.
struct OnlinePlayerRemovedAction : public GameAction<E_ACTION_ONLINE_PLAYER_REMOVED>
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // 0x00
    bool                mbIsLocalPlayerInGame;  // 0x04 (DWARF BrnGameActions.h:4161; untouched by SetActiveRaceCarIndex)

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);
};

// X360 0x823554B0 (SetProgressionRanks) / 0x82355328 (SetProgressionRankEventWins). The rank-info
// response action: the player's overall rank + per-mode ranks (0x00-0x10), then the per-mode
// rank-win counts (0x14-0x20).
struct RankInfoResponseAction : public GameAction<E_ACTION_RANK_INFO_RESPONSE>
{
    static const s32 KI_PLAYER_HAS_FINISHED_LAST_RANK = -1;

    s32 miPlayerRank;            // 0x00
    s32 miOfflineRace;           // 0x04
    s32 miRoadRage;              // 0x08
    s32 miStuntAttack;           // 0x0C
    s32 miMarkedMan;             // 0x10
    s32 miOfflineRaceRankWins;   // 0x14
    s32 miRoadRageRankWins;      // 0x18
    s32 miStuntAttackRankWins;   // 0x1C
    s32 miMarkedManRankWins;     // 0x20

    void SetProgressionRanks(s32 liPlayerRank, s32 liRankCount, s32 liOfflineRace,
                             s32 liRoadRage, s32 liStuntAttack, s32 liMarkedMan);
    void SetProgressionRankEventWins(s32 liOfflineRaceRankWins, s32 liRoadRageRankWins,
                                     s32 liStuntAttackRankWins, s32 liMarkedManRankWins);
};

// X360 element of Array<TrophyUnlockAction,12> @ 0x8235E1F0 / ::Erase @ 0x8235E318. DWARF
// BrnGameActions.h:2438; 16-byte stride (UnlockType@0x00 + 8-byte-aligned CgsID@0x08). GameAction<T>
// base carries only a static type tag (no instance data/vtable), so the struct is exactly the two members.
struct TrophyUnlockAction : public GameAction<E_ACTION_TROPHY_UNLOCK>
{
    BrnProgression::TrophyUnlockData::UnlockType meUnlockType;  // 0x00  (DWARF BrnGameActions.h:2440)
    CgsID                                        mCarToUnlock;  // 0x08  (DWARF BrnGameActions.h:2441)
};

// X360 0x8230FDF0 (Construct), 0x822A0198 (GetPlayerDisconnected), 0x8230FD60 (SetPlayerDisconnected).
// True owning home (DWARF BrnGameActions.h:853); all DWARF members/methods declared, only the three
// reconstructed bodies are defined in BrnGameActions.cpp. Layout follows DWARF source order; exact byte
// offsets are NOT X360-faithful on the x64 gate (GameModeParams / Vector3 strides differ) -- parity is
// by named member, per the BrnGameModeParams.h precedent.
struct PrepareForModeAction : public GameAction<E_ACTION_PREPARE_FOR_MODE>
{
    // DWARF BrnGameActions.h:856 (nested).
    enum EPrepareForModeStage
    {
        E_PFM_STAGE_ALL_IN_ONE    = 0,
        E_PFM_STAGE_FIRST_OF_TWO  = 1,
        E_PFM_STAGE_SECOND_OF_TWO = 2,
        E_PFM_STAGE_COUNT         = 3
    };

    // == BrnWorld::KI_MAX_ACTIVE_RACE_CARS (the disconnect asserts spell that name). Both fixed
    // arrays are [8]. KI_MAX_DISCONNECTED_NETWORK_PLAYERS is the bound the disconnect getters assert.
    static const s32 KI_MAX_PLAYERS                       = 8;
    static const s32 KI_MAX_DISCONNECTED_NETWORK_PLAYERS  = 8;

    void                  Construct(const GameModeParams* lpGameModeParams, s32 liCurrentRound,
                                    bool lbComingFromOnlineLobbyMode);   // X360 0x8230FDF0 (defined)
    const GameModeParams* GetGameModeParams() const;                     // declared-only (own ledger entry)
    s32                   GetCurrentRound() const;                       // declared-only
    bool                  IsMovingBetweenOnlineLobbyModes() const;       // declared-only
    void                  SetPlayerScoringIndex(s32 liIndex, EPlayerScoringIndex leIndex); // declared-only
    EPlayerScoringIndex   GetPlayerScoringIndex(s32 liIndex) const;      // declared-only
    bool                  GetPlayerDisconnected(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const; // X360 0x822A0198 (defined)
    void                  SetPlayerDisconnected(BrnNetwork::NetworkPlayerID lPlayerID);   // X360 0x8230FD60 (defined)
    f32                   GetPlayerBoostEarning() const;                 // declared-only
    void                  SetPlayerBoostEarning(f32 lfBoostEarning);     // declared-only
    s32                   GetShotGroup() const;                          // declared-only
    void                  SetShotGroup(s32 liShotGroup);                 // declared-only
    bool                  GetFinishedOnlineEvent() const;                // declared-only
    void                  SetFinishedOnlineEvent(bool lbFinished);       // declared-only
    bool                  IsFirstPrepareForMode() const;                 // declared-only
    void                  SetPrepareStage(EPrepareForModeStage leStage); // declared-only
    void                  SetStartingFreeburnLobbyDueToPlayerJoin(bool lbStarting); // declared-only
    bool                  GetStartingFreeburnLobbyDueToPlayerJoin() const;          // declared-only

private:
    // Data members in DWARF source order (BrnGameActions.h:936-949).
    EPrepareForModeStage        mePrepareForModeStage;                         // X360 +0x0000
    EPlayerScoringIndex         maePlayerScoringIndex[KI_MAX_PLAYERS];         // X360 +0x0004
    GameModeParams              mGameModeParams;                               // X360 +0x0030 (2160 bytes)
    s32                         miCurrentRound;                                // X360 +0x08A0 (2208)
    BrnNetwork::NetworkPlayerID maDisconnectedNetworkPlayerID[KI_MAX_PLAYERS]; // X360 +0x08A4 (2212)
    s32                         miNumPlayersDisconnected;                      // X360 +0x08C4 (2244)
    f32                         mfPlayerBoostEarning;                          // X360 +0x08C8 (2248)
    s32                         miShotGroup;                                   // X360 +0x08CC (2252)
    bool                        mbComingFromOnlineLobbyMode;                   // X360 +0x08D0 (2256)
    bool                        mbFinishedOnlineEvent;                         // X360 +0x08D1 (2257)
    bool                        mbStartingFreeburnDueToPlayerJoin;             // X360 +0x08D2 (2258)
};

// X360 action: Array<DriveThruInfo,46>::Append @ 0x8235C8E8 / ::GetLength @ 0x823AC200. Minimal slice:
// the nested DriveThruInfo element record + the fixed-capacity Array<> member that owns it
// (DWARF BrnGameActions.h:1710-1723).
struct SetUpAllDriveThrusAction : public GameAction<E_ACTION_SET_UP_ALL_DRIVE_THRUS>
{
    // DWARF BrnGameActions.h:1714-1718. 24-byte stride (verified vs the X360 element copy: 3 QWORD
    // stores into &maElements[miCount]); CgsID is u64 (8B aligned at 0x08), the trailing enum at 0x10
    // pads the record to 0x18.
    struct DriveThruInfo
    {
        f32                             mfXCoord;     // 0x00  (BrnGameActions.h:1715)
        f32                             mfZCoord;     // 0x04  (BrnGameActions.h:1716)
        CgsID                           mDriveThruId; // 0x08  (BrnGameActions.h:1717)
        BrnTrigger::GenericRegion::Type meType;       // 0x10  (BrnGameActions.h:1718)
    };

    Array<DriveThruInfo, 46> maDriveThrus;            // 0x00  (DWARF BrnGameActions.h:1723)
};

// X360 0x8231CA38 (SetPosition) / 0x82558580 (GetPosition). Per-player online-round result action:
// a position-indexed table of finishing slots plus a list of mid-round disconnects. DWARF
// BrnGameActions.h:5227 (true owning home). Minimal slice -- only the members the two reconstructed
// bodies touch are declared; Construct/GetWinner are declared-only (own ledger entries). GameAction<T>
// is an empty tag base, so instance data starts at +0x00 (matches the X360 a1[0] / (a1+8) access:
// maPlayerPosition[8] at +0x00, maDisconnectedPlayers Array<NetworkPlayerID,8> at +0x20 with its count
// word at +0x40 == a1[16]). NetworkPlayerID == s32 (committed BrnNetwork::NetworkPlayerID).
struct OnlineRoundResults : public GameAction<E_ACTION_ONLINE_ROUND_RESULT>
{
    static const s32 KI_POSITION_DISCONNECTED = -1;   // DWARF :5230 (GetPosition's disconnected sentinel)
    static const s32 KI_MAX_PLAYERS           = 8;    // == BrnWorld::KI_MAX_ACTIVE_RACE_CARS

    void                        Construct();                                                       // declared-only
    void                        SetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID, s32 liPosition); // 0x8231CA38
    s32                         GetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const;           // 0x82558580
    BrnNetwork::NetworkPlayerID GetWinner() const;                                                 // declared-only

private:
    BrnNetwork::NetworkPlayerID                        maPlayerPosition[KI_MAX_PLAYERS];   // +0x00 (DWARF :5248)
    Array<BrnNetwork::NetworkPlayerID, KI_MAX_PLAYERS> maDisconnectedPlayers;              // +0x20 (DWARF :5249)
};

// X360 online end-of-game results (DWARF home BrnGameActions.h:5153,
// : public GameAction<E_ACTION_ONLINE_GAME_RESULT>). Re-homed here from the provisional
// u32 mauWords[65] blob that used to live in BrnGameStateSharedIO.h (kept it there only because
// GameAction<>/EGameActionType weren't visible -- they are here). sizeof == 260 (65 u32 words),
// matching the old blob + the X360 operator= word count. LAYOUT IS X360-AUTHORITATIVE (the PS3
// DecFIGS DWARF lists a different/leaner field set past +0x24); the X360 anchors force round-count
// @0x28 and the EGameModeType discriminant @0x34. Methods Clear/Set*/Get*/operator= are defined in
// BrnGameActions.cpp.
struct OnlineGameResults : public GameAction<E_ACTION_ONLINE_GAME_RESULT>
{
    static const s32 KI_MAX_ROUNDS = 10;   // per-round array capacity (Clear loops 10; bounds use miNumberOfRounds)

    void Clear();                                                                          // 0x8230F178
    void SetRaceResults(s32 liRoundIndex, const f32* lpfRoundTime, f32 lfRoundDistance);    // 0x8230F220
    void GetRaceResults(s32 liRoundIndex, f32* lpfRoundTime, f32* lpfRoundDistance) const;  // 0x82580C60
    void SetStuntResults(s32 liRoundIndex, s32 liScore, s32 liMultiplier);                  // 0x8230F370
    void GetStuntResults(s32 liRoundIndex, s32* lpiScore, s32* lpiMultiplier) const;        // 0x82580DA8
    OnlineGameResults& operator=(const OnlineGameResults& lOther);                          // 0x82311C30

    // ---- DWARF-confirmed scalar header (0x00..0x24, BrnGameActions.h:5157-5167) ----
    CgsID           mCarUsed;                     // +0x00 (8)  DWARF:5157
    CgsSystem::Time mSecondsInEvent;              // +0x08 (8)  DWARF:5159 ({s32 seconds; f32 fraction})
    f32             mfMetersDriven;               // +0x10      DWARF:5160
    s32             miTakedownsFor;               // +0x14      DWARF:5162
    s32             miTakedownsAgainst;           // +0x18      DWARF:5163
    s32             miTraitorousTakedownsFor;     // +0x1C      DWARF:5164
    s32             miTraitorousTakedownsAgainst; // +0x20      DWARF:5165
    s32             miMarkedManTakedownsFor;      // +0x24      DWARF:5167
    // ---- X360-only / behaviour-anchored scalars (PS3 DWARF order diverges past +0x24) ----
    s32             miNumberOfRounds;             // +0x28  round-count (bounds: "... rounds N")
    s32             miReserved0x2C;               // +0x2C  zeroed by Clear; name not recoverable
    s32             miReserved0x30;               // +0x30  zeroed by Clear; name not recoverable
    s32             miEventType;                  // +0x34  EGameModeType discriminant (Clear -> -1)
    s32             miReserved0x38;               // +0x38  zeroed by Clear; name not recoverable
    // ---- Per-round arrays (X360 layout; 10 rounds each) ----
    CgsSystem::Time maRoundTimes[KI_MAX_ROUNDS];            // +0x3C (80) race round time
    f32             mafRoundDistances[KI_MAX_ROUNDS];       // +0x8C (40) race round distance
    s32             maiRoundStuntScores[KI_MAX_ROUNDS];     // +0xB4 (40) stunt score
    s32             maiRoundStuntMultipliers[KI_MAX_ROUNDS];// +0xDC (40) stunt multiplier
};

// X360 0x8232CEB0 (StuntModeScoring::DealWithStunt reads it). The "a world stunt element was
// performed" action: a stunt-element discriminant plus the 64-bit element key. DWARF home
// BrnGameActions.h:3469 (true owning home). Minimal slice -- exactly the two members the consumer
// reads. The GameAction<T> base is the empty tag, so meStuntElementType is at +0x00 and mId at
// +0x08. DealWithStunt branches on meStuntElementType (JUMP/SMASH/BILLBOARD, asserts >= COUNT is
// "Unknown world stunt type.") and Find/Inserts mId (the 64-bit CgsID stunt-element key) into the
// scorer's mRecentStuntElementSet to de-dupe repeated elements.
struct WorldStuntAction : public GameAction<E_ACTION_WORLD_STUNT_PERFORMED>
{
    StuntElementType meStuntElementType;  // +0x00  (DWARF BrnGameActions.h:3471)
    CgsID            mId;                  // +0x08  (DWARF BrnGameActions.h:3472)
};

// X360 0x82321530 (StuntModeScoring::DealWithPowerPark reads it). The power-park-scored result
// action: the park outcome enum plus the overall rating fed into the score multiplier. DWARF home
// BrnGameActions.h:3495 (true owning home). Minimal slice -- exactly the two members the consumer
// reads (meOutcome at +0x00 tested == E_PPO_SUCCESS, miOverallRating at +0x04 multiplied into the
// awarded score). meOutcome's enum type (BrnWorld::EPowerParkOutcome) is provisionally homed above;
// see that note.
struct PowerParkResultAction : public GameAction<E_ACTION_POWER_PARK_RESULT>
{
    BrnWorld::EPowerParkOutcome meOutcome;        // +0x00  (DWARF BrnGameActions.h:3497)
    s32                         miOverallRating;  // +0x04  (DWARF BrnGameActions.h:3498)
};

// "A stunt element has been completed" action. DWARF home BrnGameActions.h:3837 (true owning home);
// modelled here per the DWARF SHAPE -- the five named members the PS3 DecFIGS DWARF lists. The
// GameAction<T> base is the empty tag, so mID is at +0x00.
//
// GROWN -- X360/DWARF LAYOUT DIVERGENCE (consumer now LANDED): the X360 consumer that dereferences
// the in-progress stunt-element action, StuntModeScoring::DealWithInProgressStunt (0x82321710),
// reads a LARGER, convoy-shaped record that the lean 5-member DWARF layout does NOT express. The
// three convoy members below were GROWN ADDITIVELY (after the DWARF tail) at their X360-asm-proven
// offsets so the body can name them; the DWARF-named members above are NOT reordered/retyped:
//   +0x24  f32[8]  maConvoyLegDistances  (walked until a value >= flt_82CDB778)
//   +0x44  s32[8]  maConvoyMemberIds     (linear-searched for the player id; miss asserts
//                                         "Player not in this convoy!", BrnStuntModeScoring.cpp:1666)
//   +0x64  s32     miConvoyMemberCount   (the loop gate `count > 1`)
// The asm offsets are absolute (r31 = action base): 0x24, 0x44, 0x64. They are X360-only and
// DWARF-silent (the PS3 DecFIGS DWARF lists only the lean 5 members), so each carries an X360-proven
// FLAG. The 5 DWARF members end at +0x18 (mID@0x00 [u64,8B], meStuntElementType@0x08 [enum,4B],
// miCurrentCount@0x0C, miTotalCount@0x10, meCurrentGameMode@0x14 -- tail at +0x18); the convoy block
// starts at +0x24, leaving a 12-byte (+0x18..+0x23) DWARF-silent gap, modelled here as an explicit
// reserved pad so the convoy arrays land exactly at the asm-proven offsets. A static_assert below
// locks the three offsets.
//
// FLAG: maConvoyLegDistances / maConvoyMemberIds / miConvoyMemberCount + the +0x18 pad are
// X360-asm-proven and DWARF-silent (provisionally named per CXX_NAMING_CONVENTIONS); the +0x18..+0x23
// reserved region's true member(s) are unrecovered. Re-confirm names/extent when the full
// stunt-element action family is reconstructed against the X360 layout.
struct OnStuntElementCompleteAction : public GameAction<E_ACTION_ON_STUNT_ELEMENT_COMPLETE>
{
    CgsID            mID;                  // +0x00  (DWARF BrnGameActions.h:3839)
    StuntElementType meStuntElementType;   // +0x08  (DWARF BrnGameActions.h:3840)
    s32              miCurrentCount;        // +0x0C  (DWARF BrnGameActions.h:3841)
    s32              miTotalCount;          // +0x10  (DWARF BrnGameActions.h:3842)
    EGameModeType    meCurrentGameMode;     // +0x14  (DWARF BrnGameActions.h:3843; EGameModeType is
                                            //        nested in this GameStateModuleIO namespace)

    // ---- X360-only convoy block (GROWN additively; DWARF-silent; offsets asm-proven) ----
    u8               maReserved0x18[0x0C];  // +0x18 (X360-proven gap; DWARF-silent -- true member(s) unrecovered)
    f32              maConvoyLegDistances[8];   // +0x24 (X360-proven; DWARF-silent; walked vs flt_82CDB778)
    s32              maConvoyMemberIds[8];      // +0x44 (X360-proven; DWARF-silent; searched for player index)
    s32              miConvoyMemberCount;       // +0x64 (X360-proven; DWARF-silent; the count>1 gate)
};

// Lock the X360-asm-proven convoy offsets (0x24 / 0x44 / 0x64). The GameAction<T> base is the empty
// tag (no instance data/vtable), so member offsets are measured from the struct base == action base.
static_assert(offsetof(OnStuntElementCompleteAction, maConvoyLegDistances) == 0x24,
              "OnStuntElementCompleteAction::maConvoyLegDistances must be at X360 offset +0x24");
static_assert(offsetof(OnStuntElementCompleteAction, maConvoyMemberIds) == 0x44,
              "OnStuntElementCompleteAction::maConvoyMemberIds must be at X360 offset +0x44");
static_assert(offsetof(OnStuntElementCompleteAction, miConvoyMemberCount) == 0x64,
              "OnStuntElementCompleteAction::miConvoyMemberCount must be at X360 offset +0x64");
}
}
