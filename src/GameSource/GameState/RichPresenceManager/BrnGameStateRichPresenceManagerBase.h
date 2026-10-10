#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/RichPresenceManager/BrnGameStateRichPresenceManagerBase.h
// ============================================================================
// Home for BrnGameState::RichPresenceManagerBase -- the Xbox-Live rich-presence base. It tracks the
// game/score/lobby/district state across frames (caching the "old" value of each presence field) and,
// when a field changes, drives the per-field presence write through a set of pure-virtual setters that
// the platform-specific derived manager implements.
//
// SHAPE is DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../BrnGameStateRichPresenceManagerBase.h): the member run (in declared
// order + types, float32_t -> f32) and EVERY method signature are reproduced from the dwarfdump. The
// X360 asm (BrnGameStateRichPresenceManagerBase.cpp funcs) is the offset authority and CONFIRMS the
// member run: the Construct / Prepare / Set*Parameter bodies index the layout at exactly the byte
// offsets the declared member order produces (vptr@0, mpGameStateModule@4, mpModeManager@8,
// mpScoringSystem@0xC, mpNetworkRoundManager@0x10, miOldPosition@0x14, meOldRichPresenceStatus@0x18,
// meSetLobbyType@0x1C, meLobbyType@0x20, miCurrentRound@0x24, miTotalRounds@0x28, meIsRanked@0x2C,
// meOldRankedStatus@0x30, meOldDistrict@0x34, mePendingDistrict@0x38, mbPlayerIsInLobby@0x3C,
// miUserID@0x40, miUpdatePM@0x44, miDistrictChangePM@0x48).
//
// SCOPE of THIS TU's .cpp: Construct / Prepare / Update / SetGameTypeParameters /
// SetPositionParameter / SetRankedParameter / SetRoundParameter / ChangeDistrict /
// EGameModeTypeToERichPresenceState / GameParametersChanged / LocalPlayerLeftLobby /
// OnDistrictChange have bodies here, with the KE_GAME_MODES_TO_RICH_PRESENCE_STATES table Update
// reads through EGameModeTypeToERichPresenceState. Release / Destruct are declared for their own TUs.
// The eight Set* presence writers are virtual (console vtable indices: SetRichPresenceState@+0,
// SetCurrentRound@+4, SetTotalRounds@+8, SetCurrentPosition@+0xC, SetLobbyType@+0x10,
// SetRankedStatus@+0x14, SetDistrict@+0x18, SetNumberPlayers@+0x1C -- the reference virtual
// declaration order); the platform manager (RichPresenceManagerX360) overrides
// all eight.

#include "types.hpp"

#include "GameSource/GameState/BrnGameStateSharedIO.h"  // GameStateModuleIO::EGameModeType
#include "SharedClasses/World/BrnWorldRegion.h"          // BrnWorld::EDistrict

namespace BrnGameState
{
// Update names the two IO buffers BY POINTER only; forward-declare them here (real home
// BrnGameStateModuleIO.h) so this base does not pull the heavy module-IO header in. The .cpp
// includes that header to deref them.
namespace GameStateModuleIO { struct PreWorldInputBuffer; struct OutputBuffer; }

// Pointer-only collaborators -- forward declarations keep their heavy headers out of this home; the
// .cpp #includes the real homes where it dereferences them (BrnGameStateModule.h for the player
// active-race-car index, BrnScoringSystem.h for the race-position queries, BrnNetworkRoundManager.h
// for the round counters, BrnModeManager.h for the embedded-ScoringSystem fix-up in Construct).
class GameStateModule;
class ModeManager;
class ScoringSystem;
class NetworkRoundManager;

// ----------------------------------------------------------------------------
// RichPresenceManagerBase -- DWARF home BrnGameStateRichPresenceManagerBase.h:61 (polymorphic).
// ----------------------------------------------------------------------------
class RichPresenceManagerBase
{
public:
    // DWARF BrnGameStateRichPresenceManagerBase.h:106. The rich-presence state value the platform
    // presence layer displays. KE_GAME_MODES_TO_RICH_PRESENCE_STATES maps EGameModeType -> one of these.
    enum ERichPresenceStates
    {
        E_PRESENCE_STATE_IDLE                       = 0,
        E_PRESENCE_STATE_OFFLINE_BURNING_ROUTE      = 1,
        E_PRESENCE_STATE_OFFLINE_DEFAULT            = 2,
        E_PRESENCE_STATE_OFFLINE_INACTIVE           = 3,
        E_PRESENCE_STATE_OFFLINE_MARKED_MAN         = 4,
        E_PRESENCE_STATE_OFFLINE_RACE               = 5,
        E_PRESENCE_STATE_OFFLINE_ROAD_RAGE          = 6,
        E_PRESENCE_STATE_OFFLINE_ROAD_RULES         = 7,
        E_PRESENCE_STATE_OFFLINE_SHOWTIME           = 8,
        E_PRESENCE_STATE_OFFLINE_STUNT_ATTACK       = 9,
        E_PRESENCE_STATE_ONLINE_BURNING_HOME_RUN    = 10,
        E_PRESENCE_STATE_ONLINE_EVENT_PENDING       = 11,
        E_PRESENCE_STATE_ONLINE_FREE_BURN           = 12,
        E_PRESENCE_STATE_ONLINE_FREE_BURN_CHALLENGE = 13,
        E_PRESENCE_STATE_ONLINE_RACE                = 14,
        E_PRESENCE_STATE_ONLINE_ROAD_RULES          = 15,
        E_PRESENCE_STATE_COUNT                      = 16,
    };

    // DWARF BrnGameStateRichPresenceManagerBase.h:131. Ranked/unranked online-match presence flag.
    enum EGameRankedType
    {
        E_GAME_RANKED         = 0,
        E_GAME_UNRANKED       = 1,
        E_GAME_COUNT          = 2,
        // "ranked status not yet determined" -- the X360 Construct/Prepare seed
        // meIsRanked/meOldRankedStatus to 2 (== the count, used as the unset sentinel).
        E_GAME_RANKED_UNKNOWN = E_GAME_COUNT,
    };

    // ===== lifecycle =====
    // X360 0x8235A350. Cache the collaborators, seed the "old"-value sentinels, init the displayed
    // position, and register the two CPU perfmon monitors. mpScoringSystem is the ScoringSystem
    // embedded inside the passed-in ModeManager (X360: pModeManager + 0xDB0).
    void Construct(GameStateModule* lpGameStateModule, NetworkRoundManager* lpNetworkRoundManager,
                   ModeManager* lpModeManager);                                          // .cpp:72 / 0x8235A350
    // X360 0x8235A4C8. Re-seed all the cached/sentinel fields for a fresh round and assert the four
    // collaborators are present; always returns true.
    bool Prepare();                                                                      // .cpp:124 / 0x8235A4C8

    // Called by GameStateModule::PreWorldUpdate. Adopt the active controller's
    // user, then push the player count, the presence state (mode / road rules / free-burn lobby),
    // the per-mode parameters and a pending district change.
    void Update(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                GameStateModuleIO::OutputBuffer* lpOutput);                              // .cpp:362

    // Lobby / district state pushed by GameStateModule::ProcessGameEvents (bodied in the .cpp).
    void LocalPlayerLeftLobby();                                                         // .h:253
    void GameParametersChanged(GameStateModuleIO::EGameModeType leGameMode, bool lbIsRanked); // .cpp:490
    void OnDistrictChange(BrnWorld::EDistrict leDistrict);                                // .h:260

    // ----- declared for their own TUs -----
    bool Release();                                                                      // .cpp:158
    void Destruct();                                                                     // .cpp:185

    s32  GetUserID() { return miUserID; }                                                // .h:246

protected:
    // ===== per-field presence writers (pure virtual; the derived platform manager implements them --
    // the console image carries the eight RichPresenceManagerX360 overrides and no base body) =====
    // DWARF virtual order (vtable slots +0..+0x1C) -- this TU dispatches the bodied funcs through them.
    virtual void SetRichPresenceState(ERichPresenceStates leState) = 0;                      // .h:142  vtbl+0x00
    virtual void SetCurrentRound(s32 liRound) = 0;                                           // .h:147  vtbl+0x04
    virtual void SetTotalRounds(s32 liTotalRounds) = 0;                                      // .h:152  vtbl+0x08
    virtual void SetCurrentPosition(s32 liPosition) = 0;                                     // .h:157  vtbl+0x0C
    virtual void SetLobbyType(ERichPresenceStates leLobbyState) = 0;                         // .h:162  vtbl+0x10
    virtual void SetRankedStatus(EGameRankedType leRanked) = 0;                              // .h:167  vtbl+0x14
    virtual void SetDistrict(BrnWorld::EDistrict leDistrict) = 0;                            // .h:171  vtbl+0x18
    virtual void SetNumberPlayers(s32 liNumberPlayers) = 0;                                  // vtbl+0x1C

private:
    // ===== internal per-field update helpers (this TU) =====
    // X360 0x8237EFE8. Per-game-mode dispatch: pushes the position / round / ranked presence
    // parameters relevant to the current mode. Returns true if any field changed.
    bool SetGameTypeParameters();                                                        // .cpp:215 / 0x8237EFE8
    // X360 0x82372898. Push the player's current race position if it changed (race modes only).
    bool SetPositionParameter();                                                         // .cpp:263 / 0x82372898
    // X360 0x8235A6C8. Push the current/total round if either changed.
    bool SetRoundParameter();                                                            // .cpp:296 / 0x8235A6C8
    // X360 0x8235A798. Push the ranked/unranked status if it changed.
    bool SetRankedParameter();                                                           // .cpp:334 / 0x8235A798

    // Push the pending district if it differs from the last one pushed
    // (timed under the district-change monitor). Returns true if it was pushed.
    bool ChangeDistrict();                                                               // .cpp:513
    // EGameModeType -> presence state through KE_GAME_MODES_TO_RICH_PRESENCE_STATES.
    ERichPresenceStates EGameModeTypeToERichPresenceState(GameStateModuleIO::EGameModeType leGameMode); // .cpp:455

    // ===== data members (DWARF declared order + types; X360 offsets in comments) =====
    GameStateModule*    mpGameStateModule;       // .h:211  +0x04
    ModeManager*        mpModeManager;           // .h:212  +0x08
    ScoringSystem*      mpScoringSystem;         // .h:213  +0x0C
    NetworkRoundManager* mpNetworkRoundManager;  // .h:214  +0x10

    s32                 miOldPosition;           // .h:216  +0x14
    ERichPresenceStates meOldRichPresenceStatus; // .h:217  +0x18

    GameStateModuleIO::EGameModeType meSetLobbyType; // .h:218  +0x1C
    GameStateModuleIO::EGameModeType meLobbyType;    // .h:219  +0x20

    s32                 miCurrentRound;          // .h:220  +0x24
    s32                 miTotalRounds;           // .h:221  +0x28

    EGameRankedType     meIsRanked;              // .h:224  +0x2C
    EGameRankedType     meOldRankedStatus;       // .h:225  +0x30

    BrnWorld::EDistrict meOldDistrict;           // .h:227  +0x34
    BrnWorld::EDistrict mePendingDistrict;       // .h:228  +0x38

    bool                mbPlayerIsInLobby;       // .h:230  +0x3C

    s32                 miUserID;                // .h:232  +0x40

    s32                 miUpdatePM;              // .h:235  +0x44
    s32                 miDistrictChangePM;      // .h:236  +0x48
};
}
