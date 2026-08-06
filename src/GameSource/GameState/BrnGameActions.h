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
    E_ACTION_FINISHED_MODE              = 31,    // DWARF BrnGameActions.h:41
    E_ACTION_PREPARE_FOR_MODE           = 19,    // DWARF BrnGameActions.h:29
    E_ACTION_SET_UP_ALL_DRIVE_THRUS     = 40,    // DWARF BrnGameActions.h:40
    E_ACTION_ON_STUNT_ELEMENT_COMPLETE  = 53,    // DWARF BrnGameActions.h:63
    E_ACTION_WORLD_STUNT_PERFORMED      = 122,   // DWARF BrnGameActions.h:132
    E_ACTION_POWER_PARK_RESULT          = 139,   // DWARF BrnGameActions.h:149

    // X360-ATTESTED value (NOT a DWARF-only import -- both ends agree on 15):
    //   producer  GameStateModule::ProcessGameEvents @0x823A0A18, the E_EVENT_COMPLETED_STUNT
    //             (case 119) arm: `li r5, 0xF` + `li r6, 0x20` @0x823A1964/0x823A195C into
    //             VariableEventQueue<13312,16>::AddEvent @0x823A19A0.
    //   consumer  RaceCarEntityModule::HandleGameActions @0x8230BE08 `case 15`, which asserts
    //             "lpCompletedStuntAction != NULL" (BrnRaceCarEntityModule.cpp:6744) and then
    //             calls vtable +0xC0 (== slot 48, BoostStrategy::UpdateStuntBoost) on the boost
    //             manager, handing it the event payload unchanged.
    // The PS3 DWARF enumerator is also 15 (BrnGameActions.h:25), i.e. no X360 shift here --
    // consistent with every other sub-53 slot in this enum.
    E_ACTION_COMPLETED_STUNT            = 15,    // DWARF BrnGameActions.h:25 (X360-attested)

    // X360-ATTESTED value: the DWARF (PS3) enumerator is 74, but every X360 producer posts
    // `li r5, 0x4F` (79) with size 8, and the X360 consumer is HandleGameActions' `case 79`.
    // That is the SAME +5 shift this enum already records for NEW_CAR_UNLOCKED (DWARF 57 ->
    // X360 62) and CAR_UNLOCK_END (DWARF 58 -> X360 63).
    E_ACTION_CAR_SELECT_CHANGE_COLOUR   = 79,    // DWARF 74 (+5 X360)

    // Freeburn-challenge action block (ChallengeManager keystone). X360-ATTESTED values:
    // the PS3-DWARF block is 145..153, but every ChallengeManager AddEvent callsite posts
    // id == DWARF+8 with the byte size matching the DWARF struct exactly (update action
    // id 155 size 0x94, success-update id 158 size 0x10, success id 159 size 0xC,
    // completion-status id 156 size 0x108, every-player id 157 size 0x838, show-selector
    // id 160 size 8, active-challenge id 161 size 0x30, end-not-active id 154 size 1,
    // challenge id 153 size 0x20).
    E_ACTION_FREEBURN_CHALLENGE                          = 153,  // DWARF 145 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE           = 154,  // DWARF 146 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_UPDATE                   = 155,  // DWARF 147 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS        = 156,  // DWARF 148 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_EVERY_PLAYER_COMPLETION_STATUS = 157, // DWARF 149 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE           = 158,  // DWARF 150 (+8 X360)
    E_ACTION_FREEBURN_CHALLENGE_SUCCESS                  = 159,  // DWARF 151 (+8 X360)
    // StreetManager keystone (wave B). PS3-DWARF value (BrnGameActions.h enum: 262);
    // the X360 discriminant is owned by the GameStateModule dispatcher that posts the
    // query -- template tag only here (the filler, FillInRoadRulesQuery, never reads it).
    E_ACTION_ROAD_RULES_BATCH_QUERY                      = 262,  // value unconfirmed on X360 (template tag only)
    E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR            = 160,  // DWARF 152 (+8 X360)
    E_ACTION_ACTIVE_FREEBURN_CHALLENGE                   = 161,  // DWARF 153 (+8 X360)
};

template <EGameActionType T>
struct GameAction { };

// X360 0x822A0250 (HasToChangeLocation).
//
// ⭐ GROWN to the FULL 80-byte record (reset-player-car wave 2026-08-01). It was a two-member
// slice while SIX producers built the payload as raw `u8 lac[80]` + memcpy at hand-written
// offsets -- which is exactly how five of them ended up building it at the WRONG offsets
// (corrected the previous wave) and the sixth, CarSelectManager::EnterJunkyardAtStartOfGame,
// was STILL wrong when this wave started. The whole record is now typed, so the producers
// assign named members and the class of bug cannot recur.
//
// Every offset below is read (or written) at that offset by BOTH ends:
//   consumer  RaceCarEntityModule::HandleResetPlayerCarAction @0x82304FE8
//             (lvx128 r16+0x00 / r16+0x10; ld 0x20 / 0x28; lwz 0x30 / 0x38 / 0x3C;
//              lfs 0x34; lbz 0x40 / 0x41 / 0x43)
//   producer  CarSelectManager::EnterJunkyardAtStartOfGame @0x82393080 and the five
//             CarSelectManager builders at 0x82387D38 / 0x82392F78 / 0x82398B3C /
//             0x82392D64 / 0x82398DDC.
// Sizeof is pinned at 80 by the AddEvent liSize every producer passes (`li r6, 0x50`).
struct alignas(16) ResetPlayerCarAction : public GameAction<E_ACTION_RESET_PLAYER_CAR>
{
    Vector3 mPosition;                 // +0x00  SpawnLocation::mPosition
    Vector3 mDirection;                // +0x10  SpawnLocation::mDirection
    CgsID   mCarModelId;               // +0x20  0 == "do not re-spawn, just teleport"
    CgsID   mWheelModelId;             // +0x28  0 == "derive from the car's default wheel name"
    EPlayerScoringIndex mePlayerScoringIndex; // +0x30  >= E_PLAYER_SCORING_INDEX_COUNT == none
    f32     mfDeformationAmount;       // +0x34  < 0 == "no unlock deform" (the -1.0 sentinel)
    f32     mfDeformationAmount2;      // +0x38  FLAG: the consumer stores the PAIR (+0x34,+0x38)
                                       //        into ActiveRaceCar[499]/[498] and into the
                                       //        module's own two deform scalars, and only when
                                       //        +0x34 >= 0. EnterJunkyardAtStartOfGame leaves
                                       //        this field UNWRITTEN (its -0x50 record is not
                                       //        memset), which is safe precisely because it
                                       //        posts the -1.0 sentinel. Name provisional.
    s32     miInCarModification;       // +0x3C  copied to the module's +0x186CC word.
                                       //        TeleportCurrentVehicle sets it on the in-car-mod
                                       //        path, UpdateUnlockState posts 2. Name provisional.
    bool    mbInCarSelectScreen;       // +0x40  -> RaceCarEntityModule::mbInCarSelectScreen
    bool    mbCarSelectDontStreamAudio;// +0x41  -> RaceCarEntityModule::mbCarSelectDontStreamAudio
    u8      muReserved0x42;            // +0x42  written 0 by every producer; NO consumer reads it
    bool    mbKeepResetSection;        // +0x43  -> SpawnRaceCar's 5th arg -> the DWARF-named
                                       //        AttachAIControlEvent::mbKeepResetSection
    u8      maReserved0x44[0x0C];      // +0x44..+0x4F  tail of the 80-byte AddEvent image

    bool HasToChangeLocation() const;
};
static_assert(sizeof(ResetPlayerCarAction) == 80,
              "ResetPlayerCarAction must be the 80-byte image every AddEvent(.., 0, 80) posts");

// Game action 64 -- "the player's car selection changed": which junkyard, and where in it.
// TYPED 2026-08-01 (reset-player-car wave); it was an opaque forward declaration while three
// producers stamped it by hand and MainDirector::ProcessInputQueue read it by hand -- which is
// how the +0x30 "pos is left" bit ended up being written at +0x10 for a whole campaign.
// Both ends agree on every offset below:
//   producer  CarSelectManager::SpawnInStartCar @0x82387D9C..0x82387E18 (AddEvent .., 64, 64)
//             CarSelectManager::EnterJunkyardAtStartOfGame @0x82393178..0x823931E0 (by pointer)
//   consumer  MainDirector::ProcessInputQueue case 64 (BrnMainDirector.cpp:753, X360
//             `lbz r11, 0x30(r30)` @0x82238418 -> maGameState.mbJunkyardPosIsLeft)
struct alignas(16) CarSelectionChangedAction
{
    CgsID   mJunkyardId;        // +0x00
    u8      maReserved0x08[8];  // +0x08  never written by any producer
    Vector3 mPosition;          // +0x10  SpawnLocation::mPosition
    Vector3 mDirection;         // +0x20  SpawnLocation::mDirection
    bool    mbJunkyardPosIsLeft;// +0x30  == (SpawnLocation::GetType() == E_TYPE_CAR_SELECT_LEFT)
    bool    mbReserved0x31;     // +0x31  written 0
    u8      maReserved0x32[14]; // +0x32..+0x3F  tail of the 64-byte AddEvent image
};
static_assert(sizeof(CarSelectionChangedAction) == 64,
              "CarSelectionChangedAction must be the 64-byte image AddEvent(.., 64, 64) posts");
static_assert(offsetof(CarSelectionChangedAction, mbJunkyardPosIsLeft) == 0x30,
              "CarSelectionChangedAction::mbJunkyardPosIsLeft at +0x30 (the consumer's lbz offset)");

// Game action 79 -- "paint the player's car": the palette (paint finish) and colour indices the
// car-select / junkyard flow resolved for the car that is now in the world. It is the ONLY route
// by which a car's AUTHORED default colour reaches BrnWorld::RaceCar: HandleResetPlayerCarAction
// spawns every fresh car at 0/0 (it only carries an OLD car's colour across a same-model
// respawn), and this action is what the console posts immediately afterwards.
//
// ⭐ PALETTE FIRST. Both ends are attested and they agree:
//   producer  CarSelectManager::ReallyEnterJunkyardAtStartOfGame @0x823931F8
//             (`r5 = &sp+0x5C` == the COLOUR out-param, `r6 = &sp+0x58` == the PALETTE
//              out-param, then `AddEvent(sp+0x58, 79, 8)` -- the payload BASE is the palette),
//             CarSelectManager::UpdateUnlockState @0x82398920 and
//             CarSelectManager::TeleportCurrentVehicle @0x82392EF0
//             (both `GetCarColourAndPalette(.., &v + 4, &v); AddEvent(&v, 79, 8)`).
//   consumer  RaceCarEntityModule::ChangePlayerCarColour @0x822D27B0, reached from
//             HandleGameActions' `case 79` as `ChangePlayerCarColour(payload[0], payload[4])`,
//             which stores arg0 to RaceCar+152 (miColourPalette) and arg1 to +148
//             (miColourIndex).
// The member names and widths are the DWARF's (BrnGameActions.h:2811 CarSelectChangeColourAction
// : GameAction<E_ACTION_CAR_SELECT_CHANGE_COLOUR> { uint32_t muPaletteIndex; uint32_t
// muColourIndex; }) -- which independently puts the palette first.
//
// ⚠️ NOT to be confused with the DWARF's CarUnlockAction: that is
// GameAction<E_ACTION_GUI_CAR_UNLOCK> (PS3 id 2) and holds a single `CgsID mCurrentCarToUnlock`
// -- the other 8-byte event UpdateUnlockState posts (`AddEvent(&v17, 2, 8)`).
struct CarSelectChangeColourAction : public GameAction<E_ACTION_CAR_SELECT_CHANGE_COLOUR>
{
    u32 muPaletteIndex;   // +0x00
    u32 muColourIndex;    // +0x04
};
static_assert(sizeof(CarSelectChangeColourAction) == 8,
              "CarSelectChangeColourAction must be the 8-byte image AddEvent(.., 79, 8) posts");
static_assert(offsetof(CarSelectChangeColourAction, muPaletteIndex) == 0,
              "the palette index is payload word 0 (ChangePlayerCarColour's first argument)");
static_assert(offsetof(CarSelectChangeColourAction, muColourIndex) == 4,
              "the colour index is payload word 1 (ChangePlayerCarColour's second argument)");

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

// X360 BrnNetwork::StandingsManager::HandlePlayerFinishedMode (0x82550BB8) reads it; the per-player
// "this player has finished the mode/round" action. DWARF home BrnGameActions.h:1347 (the true owning
// home for the FULL member set + methods; only the SHAPE is modelled here so the StandingsManager TU
// can name the fields it dereferences). The GameAction<T> base is the empty tag, so mFinishTime is at
// +0x00, matching the X360 access pattern (HandlePlayerFinishedMode reads a3+0/+4 == mFinishTime,
// a3+20 == meEliminatorIndex, a3+32 == mfDistanceFromFinish, a3+40 == miEliminations,
// a3+45 == mbTimedOut, a3+46 == mbWonRound). Member order/offsets follow the DWARF source list
// (mFinishTime@0, mFastestLapTime@8, meFinishedGameModeType@16, meEliminatorIndex@20,
// meBeatenRivalIndex@24, miNumberOfTakedowns@28, mfDistanceFromFinish@32, miFinishPosition@36,
// miEliminations@40, mbIsOnlineGameMode@44, mbTimedOut@45), all of which the X360 asm corroborates.
//
// X360-ATTESTED, DWARF-OMITTED member: the binary reads a SEVENTH-from-end byte at +0x2E (a3+46)
// immediately after mbTimedOut and copies it into the per-player StandingsData "won round" byte and
// forwards it to PlayerFinishedRoundMessage::PrepareForSend's last argument. The PS3 DecFIGS DWARF
// models this action with mbTimedOut as its last bool and omits the +46 byte; the X360 build clearly
// carries it. Named mbWonRound here (FLAGGED -- the same round-outcome companion bool seen on
// BrnNetwork::PlayerFinishedRoundMessage; source name not independently attested).
struct FinishedModeAction : public GameAction<E_ACTION_FINISHED_MODE>
{
    CgsSystem::Time     mFinishTime;             // +0x00 (8)  DWARF BrnGameActions.h:1348
    CgsSystem::Time     mFastestLapTime;         // +0x08 (8)  DWARF BrnGameActions.h:1349
    EGameModeType       meFinishedGameModeType;  // +0x10      DWARF BrnGameActions.h:1351
    EActiveRaceCarIndex meEliminatorIndex;       // +0x14      DWARF BrnGameActions.h:1352 (a3+20)
    EGlobalRaceCarIndex meBeatenRivalIndex;      // +0x18      DWARF BrnGameActions.h:1353
    s32                 miNumberOfTakedowns;     // +0x1C      DWARF BrnGameActions.h:1355
    f32                 mfDistanceFromFinish;    // +0x20      DWARF BrnGameActions.h:1356 (a3+32)
    s32                 miFinishPosition;        // +0x24      DWARF BrnGameActions.h:1357
    s32                 miEliminations;          // +0x28      DWARF BrnGameActions.h:1358 (a3+40)
    bool                mbIsOnlineGameMode;      // +0x2C      DWARF BrnGameActions.h:1360
    bool                mbTimedOut;              // +0x2D      DWARF BrnGameActions.h:1361 (a3+45)
    bool                mbWonRound;              // +0x2E      X360-attested, DWARF-omitted (FLAGGED name) (a3+46)
};

// Lock the X360-asm-proven convoy offsets (0x24 / 0x44 / 0x64). The GameAction<T> base is the empty
// tag (no instance data/vtable), so member offsets are measured from the struct base == action base.
static_assert(offsetof(OnStuntElementCompleteAction, maConvoyLegDistances) == 0x24,
              "OnStuntElementCompleteAction::maConvoyLegDistances must be at X360 offset +0x24");
static_assert(offsetof(OnStuntElementCompleteAction, maConvoyMemberIds) == 0x44,
              "OnStuntElementCompleteAction::maConvoyMemberIds must be at X360 offset +0x44");
static_assert(offsetof(OnStuntElementCompleteAction, miConvoyMemberCount) == 0x64,
              "OnStuntElementCompleteAction::miConvoyMemberCount must be at X360 offset +0x64");

// ===== Freeburn-challenge actions (ChallengeManager keystone) =====

// DWARF BrnGameActions.h:5543 (member set + order). X360 size attested: ChallengeManager::
// WriteDataToOutput posts it with AddEvent(..., 155, 0x94) and 4+4+4+64+8+64 == 0x94 exactly.
// [2][8] == [action][player] (note the transposed order vs the manager's [player][action] grids).
struct FreeburnChallengeUpdateAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_UPDATE>
{
    f32 mfTimeLeftInChallenge;                            // 0x00 (:5546)
    s32 miCurrentActionIndex;                             // 0x04 (:5547)
    s32 miNumTargetsUsed;                                 // 0x08 (:5549)
    f32 maafIndividualTargetContributions[2][8];          // 0x0C (:5550)
    s32 maiOverallTargetRemaining[2];                     // 0x4C (:5551)
    EFreeburnChallengeSuccess maaeCompleted[2][8];        // 0x54 (:5552)
}; // sizeof == 0x94

// DWARF BrnGameActions.h:5590. X360 size attested: UpdateActionSuccess posts AddEvent(..., 158, 0x10).
struct FburnChallengeSuccessUpdateAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE>
{
    // DWARF also spells the LastSecondChallengeSuccess typedef at :3142 inside this struct;
    // the owning definition is BrnGameStateSharedIO.h:313 (FastBitArray<60>).
    LastSecondChallengeSuccess mChallengeSuccessUpdate; // 0x00 (:5592, FastBitArray<60>)
    s32 miActionIndex;                                  // 0x08 (:5593)
}; // sizeof == 0x10

// DWARF BrnGameActions.h:5604. X360 size attested: UpdateChallenge posts AddEvent(..., 159, 0xC).
struct FburnChallengeSuccessAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_SUCCESS>
{
    f32  mafActionScores[2];          // 0x00 (:5606)
    bool mabSuccessfulActions[2];     // 0x08 (:5607)
    bool mabAccumulationThisFrame[2]; // 0x0A (:5608)
}; // sizeof == 0xC

// The plain freeburn-challenge lifecycle action (id 153). DWARF BrnGameActions.h:5514 (member
// set + order). X360 size attested: SEVEN ChallengeManager sites post it with
// AddEvent(..., 153, 0x20) -- BeginChallenge 0x823505B8, TriggerFreeburnChallenge 0x82346DA0,
// EndChallenge 0x8234DE30, UpdateResults 0x82345FD0, UpdateArbitration 0x82351AF8,
// UpdateArbitrationSuccess 0x82350340, UpdateChallenge 0x82347190 -- and every field store below
// is offset-attested at those sites (CgsID @0x00, s32 @0x08/0x0C/0x10/0x14/0x18, byte @0x1C/0x1D,
// 2B tail pad -> 0x20).
// meEventType is BrnNetwork::BrnNetworkModuleIO::EChallengeEventType (DWARF :5517) stored as s32
// (same 4B storage; the StartNetworkGameEvent::meBoostType precedent -- including the heavy
// BrnNetworkModuleIO.h here is not warranted). X360-DRIFT WARNING on its VALUES: the begin(0)/
// trigger(1)/action-success(2)/reset(3) posts match the committed PS3-DWARF enumerators 1:1, but
// EndChallenge stores 5 and UpdateResults stores 6 where the PS3 enum spells ENDED=4/
// RESULTS_FINISHED=5 -- the X360 enum carries one extra (unattested) enumerator before ENDED.
// Store the raw attested value with a drift comment; do NOT "fix" 5/6 back to the PS3 names.
struct FreeburnChallengeAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE>
{
    CgsID mChallengeID;                  // 0x00 (:5516)
    s32   meEventType;                   // 0x08 (:5517, BrnNetworkModuleIO::EChallengeEventType; s32 storage)
    EChallengeStatus meChallengeStatus;  // 0x0C (:5518, BrnGameState::EChallengeStatus)
    s32   miActionIndex;                 // 0x10 (:5519)
    s32   miNumChallengesComplete;       // 0x14 (:5520; -1 == "not supplied" at most sites)
    s32   miTotalNumChallenges;          // 0x18 (:5521; -1 == "not supplied" at most sites)
    bool  mbIsHost;                      // 0x1C (:5522)
    bool  mbAbortingToStartNewChallenge; // 0x1D (:5523)

    void Construct();                    // :5527 declared-only (not in this TU's X360 ledger)
}; // sizeof == 0x20

// The empty end-not-active notification action (id 154). DWARF BrnGameActions.h:5531 (an EMPTY
// struct -- no members). X360 size attested: CancelFreeburnChallenge / RemoteEndChallenge /
// UpdateArbitrationSuccess post AddEvent(..., 154, 1) from an uninitialised stack byte.
// (The already-committed partfiles post the bare GameAction<E_ACTION_FREEBURN_CHALLENGE_END_
// NOT_ACTIVE> tag, which is layout-identical; prefer this named struct in new bodies.)
struct FburnChallengeEndNotActiveAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE>
{
}; // sizeof == 1 (empty)

// One player's freeburn-challenge completion-status action (id 156). DWARF BrnGameActions.h:5563.
// X360 size attested: NetworkPlayerFinalised 0x82347E88 posts AddEvent(..., 156, 0x108) after
// memcpy'ing the manager's 256-byte local completion bit store to +0x00 and stamping the player
// id at +0x100 (stw sp+0x180). NOTE the field ORDER differs from CompletedFburnChallengesData
// (the EventQueue element, id first) -- here the bit store leads.
struct FburnChallengeStatusAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS>
{
    CompletedFburnChallenges    mCompletedChallenges; // 0x000 (:5565, FastBitArray<2000> == 256B)
    BrnNetwork::NetworkPlayerID mPlayerID;            // 0x100 (:5566)
}; // sizeof == 0x108 (4B tail pad)

// The "show the challenge selector" GUI action (id 160). DWARF BrnGameActions.h:5619.
// X360 size attested: NetworkPlayerRemoved 0x8234E420 (posts it ZEROED -- std r0 -> mChallengeID
// == 0), Disconnected-family and UpdateArbitration / UpdateArbitrationSuccess post
// AddEvent(..., 160, 8).
struct FburnChallengeShowSelectorAction : public GameAction<E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR>
{
    CgsID mChallengeID;   // 0x00 (:5621)
}; // sizeof == 8

// The "this is the currently active freeburn challenge + its participants" action (id 161).
// DWARF BrnGameActions.h:5632 (member SET; see drift note). X360 size + layout attested:
// NetworkPlayerFinalised 0x82347E88 builds it on the stack (sp+0x50) and posts
// AddEvent(..., 161, 0x30): CgsID @0x00 (std, mpCurrentChallenge id), the ARCI slots filled from
// +0x08 (stwx r30, 4*count + sp+0x58), the destination player id @0x24 (stw sp+0x74) and the
// participant count @0x28 (stw sp+0x78).
// X360-vs-DWARF MEMBER-ORDER DRIFT: the PS3 DWARF lists maePlayersInChallengeARCI FIRST (id at
// +0x20) -- the X360 build stores the id at +0x00 and the ARCI block at +0x08. Member names are
// the DWARF's; the ORDER below is X360-authoritative. (The matching *event* consumed by
// ProcessEvent case 173 KEEPS the PS3 order -- see ActiveFburnChallengeEvent in BrnGameEvents.h.)
struct ActiveFburnChallengeAction : public GameAction<E_ACTION_ACTIVE_FREEBURN_CHALLENGE>
{
    // == the count the NetworkPlayerFinalised bound assert spells:
    // "lActiveChallengeAction.miNumPlayersInChallenge < KI_MAX_NETWORK_PLAYERS"
    static const s32 KI_MAX_NETWORK_PLAYERS = 7;

    CgsID                       mChallengeID;                // 0x00 (:5635; X360 FIRST -- drift)
    EActiveRaceCarIndex         maePlayersInChallengeARCI[KI_MAX_NETWORK_PLAYERS]; // 0x08..0x23 (:5634)
    BrnNetwork::NetworkPlayerID mPlayerToSendToID;           // 0x24 (:5636)
    s32                         miNumPlayersInChallenge;     // 0x28 (:5637)
}; // sizeof == 0x30 (4B tail pad)

// Pin the X360-attested offsets of the freeburn action family (all pointer-free -> absolute on
// the x64 gate too). GameAction<T> is the empty tag base, so struct base == action base.
static_assert(sizeof(FreeburnChallengeAction) == 0x20, "FreeburnChallengeAction must be 0x20 bytes (AddEvent size)");
static_assert(offsetof(FreeburnChallengeAction, meEventType) == 0x08, "FreeburnChallengeAction::meEventType at +0x08");
static_assert(offsetof(FreeburnChallengeAction, mbIsHost) == 0x1C, "FreeburnChallengeAction::mbIsHost at +0x1C");
static_assert(sizeof(FburnChallengeStatusAction) == 0x108, "FburnChallengeStatusAction must be 0x108 bytes (AddEvent size)");
static_assert(offsetof(FburnChallengeStatusAction, mPlayerID) == 0x100, "FburnChallengeStatusAction::mPlayerID at +0x100");
static_assert(sizeof(FburnChallengeShowSelectorAction) == 8, "FburnChallengeShowSelectorAction must be 8 bytes (AddEvent size)");
static_assert(sizeof(ActiveFburnChallengeAction) == 0x30, "ActiveFburnChallengeAction must be 0x30 bytes (AddEvent size)");
static_assert(offsetof(ActiveFburnChallengeAction, maePlayersInChallengeARCI) == 0x08, "ActiveFburnChallengeAction ARCI block at +0x08");
static_assert(offsetof(ActiveFburnChallengeAction, mPlayerToSendToID) == 0x24, "ActiveFburnChallengeAction::mPlayerToSendToID at +0x24");
static_assert(offsetof(ActiveFburnChallengeAction, miNumPlayersInChallenge) == 0x28, "ActiveFburnChallengeAction::miNumPlayersInChallenge at +0x28");

// ===== Road-rules batch query (StreetManager keystone, wave B) =====
// ADDITIVE GROW. DWARF BrnGameActions.h:2833 (members :4808..:4814); the member ORDER
// below is X360-authoritative from StreetManager::FillInRoadRulesQuery @ 0x823365A8,
// which stores miNumRoads at +512 (right after the ids) and the four bool arrays at
// +516/+580/+644/+708 with the owns-all flag at +772 -- the PS3 DWARF lists miNumRoads
// after the bool arrays; the X360 build places it before them.
// The two "beaten" pairs are indexed by BrnStreetData::ScoreType (0 == TIME, 1 == CRASH).
struct RoadRulesBatchQueryAction : public GameAction<E_ACTION_ROAD_RULES_BATCH_QUERY>
{
    ::CgsID maRoadIds[64];                 // +0    (:4808) one CgsID per road
    s32     miNumRoads;                    // +512  (:4813) asserted <= KI_MAX_CHALLENGES
    bool    mabPlayerBeatenParTime[64];    // +516  (:4809) HasPlayerBeatenParScore(i, TIME)  == BEATEN
    bool    mabPlayerBeatenParCrash[64];   // +580  (:4810) HasPlayerBeatenParScore(i, CRASH) == BEATEN
    bool    mabPlayerBestOnlineTime[64];   // +644  (:4811) HasPlayerBeatenFriendScore(i, TIME)  == BEATEN
    bool    mabPlayerBestOnlineCrash[64];  // +708  (:4812) HasPlayerBeatenFriendScore(i, CRASH) == BEATEN
    bool    mbPlayerOwnsAllRoadsOffline;   // +772  (:4814) ProgressionManager complete-roads tally >= 64
};

// ===== Completed-stunt action (id 15) =====
// The one-shot "the player just finished a stunt" record. DWARF home BrnGameActions.h:782
// (`struct CompletedStuntAction : public GameAction<E_ACTION_COMPLETED_STUNT>`), which is this
// file -- so this is the TRUE owning home, not a re-home.
//
// It is a straight repack of the physics-side CompletedStuntEvent (BrnGameEvents.h) down to the
// eight fields the game-side consumers actually need. Every offset below is attested at BOTH ends.
//
// PRODUCER -- GameStateModule::ProcessGameEvents @0x823A0A18, the `case 119`
// (E_EVENT_COMPLETED_STUNT) arm. r25 == the incoming CompletedStuntEvent*, r1+var_1C60 == the
// 32-byte payload base handed to AddEvent (`addi r4, r1, var_1C60` @0x823A196C):
//     0x823A1950  lwz  r11, 0x00(r25)  -> 0x823A1978  stw  +0x00
//     0x823A1954  lfs  f0,  0x58(r25)  -> 0x823A1958  stfs +0x04
//     0x823A1960  lfs  f0,  0x5C(r25)  -> 0x823A1968  stfs +0x08   (event mfCompletedAirSpinAngle)
//     0x823A1970  lfs  f0,  0x60(r25)  -> 0x823A197C  stfs +0x0C
//     0x823A1984  lfs  f0,  0x64(r25)  -> 0x823A1988  stfs +0x10
//     0x823A198C  lfs  f0,  0x68(r25)  -> 0x823A1990  stfs +0x14   (event mfCompletedDriftDistance)
//     0x823A1998  lwz  r11, 0x50(r25)  -> 0x823A199C  stw  +0x18   (event miCompletedBarrelRolls)
//     0x823A1980  lbz  r11, 0x80(r25)  -> 0x823A1994  stb  +0x1C   (event mbSuccessfulLanding)
//   then `li r5, 0xF` (action id 15) + `li r6, 0x20` (SIZE 32) -> AddEvent @0x823A19A0.
//   Two of those source offsets are already NAMED on the committed CompletedStuntEvent
//   (+0x5C mfCompletedAirSpinAngle, +0x68 mfCompletedDriftDistance, +0x50 miCompletedBarrelRolls,
//   +0x80 mbSuccessfulLanding), and they land on the DWARF members of the same name here -- which
//   is what pins the ORDER of the float run, not just its extent.
//
// CONSUMERS -- the three BoostStrategy::UpdateStuntBoost overrides (vtable slot 48), which are
// byte-identical in shape: BoostBurnout2 @0x822A6478, BoostBurnout3 @0x822A6708,
// BoostBurnout5 @0x822A6F98. Taking BoostBurnout2 (r30 == the action, r31 == this):
//     0x822A64BC  lwz  r11, 0x00(r30)  -- the FIRST member; the empty GameAction<> base adds no
//                                         offset, exactly as for every sibling in this file
//     0x822A64C0  clrlwi r11, r11, 31          bit 0 -> pays mfBarrelRollEarning
//     0x822A64CC  lwz  r11, 0x18(r30) ; 0x822A64DC extsw   -- a SIGN-EXTENDED WORD, i.e. int32
//     0x822A6504  rlwinm r11, r11, 0,30,30     bit 1
//     0x822A6518  lfs  f13, 0x08(r30)          -> pays mfAirSpinEarning
//     0x822A6534  rlwinm r11, r11, 0,29,29     bit 2
//     0x822A6548  lfs  f13, 0x0C(r30)          -> pays mfHandbrake180Earning
//     0x822A6564  rlwinm r11, r11, 0,28,28     bit 3 (flat award, reads no payload)
//   (BoostBurnout3 does the same reads at 0x822A675C / 0x822A67A8 / 0x822A67D8; BoostBurnout5 at
//    0x822A6FEC / 0x822A7038 / 0x822A7068.)
//
// ORDER NOTE -- a real X360-vs-DWARF divergence. The PS3 DWARF lists `bool mbSuccessfulLanding`
// (:791) BEFORE `int32_t miCompletedBarrelRolls` (:793), which would put the bool at +0x18 and the
// int at +0x1C. The X360 build is the other way round and says so twice, independently: the
// producer `stb`s the landing byte to +0x1C and `stw`s the roll count to +0x18 (0x823A1994 /
// 0x823A199C), and all three consumers `lwz`+`extsw` a full word from +0x18. The X360 order below
// is therefore authoritative (rung 1 over rung 2); do not "correct" it back to the DWARF.
//
// muStuntActionComplete is left as the raw u32 the DWARF declares -- it is the physics detector's
// bitfield and its enumerators already have a recovered home in
// GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManagerShared.h
// (`BrnPhysics::EStuntActionComplete`, DWARF-verbatim), whose BARREL_ROLL/AIR_SPIN/HANDBREAK_TURN/
// CLEANLANDING == 1/2/4/8 are exactly the four bits the consumers above test. Consumers mask with
// that enum (or with the literals) -- no parallel enum is minted here.
struct CompletedStuntAction : public GameAction<E_ACTION_COMPLETED_STUNT>
{
    u32  muStuntActionComplete;         // +0x00 (:784) BrnPhysics::EStuntActionComplete bitfield
    f32  mfCompletedBarrelRollAngle;    // +0x04 (:786)
    f32  mfCompletedAirSpinAngle;       // +0x08 (:787)
    f32  mfCompletedHandbreakTurnAngle; // +0x0C (:788)
    f32  mfCompletedDriftTime;          // +0x10 (:789)
    f32  mfCompletedDriftDistance;      // +0x14 (:790)
    s32  miCompletedBarrelRolls;        // +0x18 (:793) X360 order -- see the ORDER NOTE above
    bool mbSuccessfulLanding;           // +0x1C (:791) X360 order -- see the ORDER NOTE above
                                        // +0x1D..+0x1F tail pad of the 32-byte AddEvent image
};

// Pointer-free -> the X360 offsets are absolute on the x64 gate too.
static_assert(sizeof(CompletedStuntAction) == 32,
              "CompletedStuntAction must be the 32-byte image AddEvent(.., 15, 32) posts");
static_assert(offsetof(CompletedStuntAction, muStuntActionComplete) == 0x00,
              "the stunt bitfield is the first member (BoostBurnout2::UpdateStuntBoost lwz 0(r30))");
static_assert(offsetof(CompletedStuntAction, mfCompletedAirSpinAngle) == 0x08,
              "air-spin angle at +0x08 (UpdateStuntBoost lfs f13, 8(r30))");
static_assert(offsetof(CompletedStuntAction, mfCompletedHandbreakTurnAngle) == 0x0C,
              "handbrake-turn angle at +0x0C (UpdateStuntBoost lfs f13, 0xC(r30))");
static_assert(offsetof(CompletedStuntAction, miCompletedBarrelRolls) == 0x18,
              "barrel-roll COUNT at +0x18 (UpdateStuntBoost lwz+extsw 0x18(r30)) -- not the DWARF's bool");
static_assert(offsetof(CompletedStuntAction, mbSuccessfulLanding) == 0x1C,
              "landing flag at +0x1C (ProcessGameEvents stb @0x823A1994)");
}
}
