// ============================================================================
// b5-decomp/src/GameSource/GameState/RichPresenceManager/BrnGameStateRichPresenceManagerBase.cpp
// ============================================================================
// Bodies for the six RichPresenceManagerBase functions this TU owns:
//   Construct / Prepare / SetGameTypeParameters / SetPositionParameter /
//   SetRankedParameter / SetRoundParameter.
// (All other declared methods are bodied by their own TUs in later waves.)
//
// X360 asm is the offset authority (see the per-function provenance comments). The
// per-field Set*Parameter helpers each cache the field's last-pushed value and, when it
// changes, push the new value to the platform presence layer through the virtual setters.

#include "GameSource/GameState/RichPresenceManager/BrnGameStateRichPresenceManagerBase.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "GameSource/GameState/BrnGameStateModule.h"                  // GameStateModule::GetPlayerActiveRaceCarIndex
#include "GameSource/GameState/ModeManager/BrnModeManager.h"          // ModeManager::GetScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // ScoringSystem::GetNumberOfActiveCars / GetCarRacePosition
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // NetworkRoundManager round counters

namespace BrnGameState
{
namespace
{
    // X360 perfmon monitor registration constants (Construct's two AddMonitor calls). Colour 5,
    // 1.0 ms budget, libperf-tagged; the X360 leaves the parent-handle register (r6) unset -- it is
    // an indeterminate "no parent" handle, modelled here as the invalid handle sentinel.
    const s32    KI_RICH_PRESENCE_PM_COLOUR     = 5;
    const f32    KF_RICH_PRESENCE_PM_BUDGET_MS  = 1.0f;
    const s32    KI_RICH_PRESENCE_PM_NO_PARENT  = -1;

    // X360 sentinel stored into meSetLobbyType / meLobbyType to mark "no lobby type set yet". It sits
    // one past the last EGameModeType enumerator (E_MODE_COUNT == 17), i.e. an out-of-band invalid mode.
    const s32    KI_INVALID_LOBBY_TYPE          = 18;
}

// X360 0x8235A350. Construct the rich-presence manager: cache the three collaborators, derive the
// scoring system from the ModeManager's embedded one, seed every cached "old"-value sentinel, init the
// displayed position to 1, push it through SetCurrentPosition, then register the two CPU perfmon
// monitors. The four collaborator null-asserts mirror the X360 (the X360 bakes file/line; CGS_ASSERT
// injects this file's __FILE__/__LINE__).
void RichPresenceManagerBase::Construct(GameStateModule* lpGameStateModule,
                                        NetworkRoundManager* lpNetworkRoundManager,
                                        ModeManager* lpModeManager)
{
    mpGameStateModule    = lpGameStateModule;
    meLobbyType          = static_cast<GameStateModuleIO::EGameModeType>(KI_INVALID_LOBBY_TYPE);
    meSetLobbyType       = static_cast<GameStateModuleIO::EGameModeType>(KI_INVALID_LOBBY_TYPE);
    miUserID             = -1;
    meOldRichPresenceStatus = E_PRESENCE_STATE_IDLE;
    mpNetworkRoundManager = lpNetworkRoundManager;
    mpModeManager        = lpModeManager;
    // The ScoringSystem is embedded by value inside the ModeManager (X360 pModeManager + 0xDB0);
    // GetScoringSystem() is the named accessor for that embedded sub-object.
    mpScoringSystem      = lpModeManager ? lpModeManager->GetScoringSystem() : nullptr;

    CGS_ASSERT(mpGameStateModule, "mpGameStateModule");
    CGS_ASSERT(mpNetworkRoundManager, "mpNetworkRoundManager");
    CGS_ASSERT(mpModeManager, "mpModeManager");
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    miCurrentRound       = -1;
    miTotalRounds        = -1;
    mbPlayerIsInLobby    = false;
    meIsRanked           = E_GAME_RANKED_UNKNOWN;   // X360 li 2 -> meIsRanked(+0x2C)
    meOldRankedStatus    = E_GAME_RANKED_UNKNOWN;   // X360 li 2 -> meOldRankedStatus(+0x30)
    miOldPosition        = 1;
    SetCurrentPosition(1);

    miUpdatePM = CgsDev::PerfMonCpu::AddMonitor("Rich presence update",
                                                KI_RICH_PRESENCE_PM_COLOUR, 0,
                                                KF_RICH_PRESENCE_PM_BUDGET_MS,
                                                KI_RICH_PRESENCE_PM_NO_PARENT, 1);
    miDistrictChangePM = CgsDev::PerfMonCpu::AddMonitor("District change",
                                                        KI_RICH_PRESENCE_PM_COLOUR, 0,
                                                        KF_RICH_PRESENCE_PM_BUDGET_MS,
                                                        KI_RICH_PRESENCE_PM_NO_PARENT, 1);
}

// X360 0x8235A4C8. Re-seed all the cached / sentinel fields for a fresh round, then assert the four
// collaborators are present (the X360 streams a richer "Did you forget to construct..." message into
// the assert buffer; CGS_ASSERT forwards the equivalent string). Always returns true.
bool RichPresenceManagerBase::Prepare()
{
    miUserID              = -1;
    miCurrentRound        = -1;
    miTotalRounds         = -1;
    meOldRichPresenceStatus = E_PRESENCE_STATE_IDLE;   // X360 stw 0, 0x18 (NOT a +0x14 miOldPosition store)
    meLobbyType           = static_cast<GameStateModuleIO::EGameModeType>(KI_INVALID_LOBBY_TYPE);
    meSetLobbyType        = static_cast<GameStateModuleIO::EGameModeType>(KI_INVALID_LOBBY_TYPE);
    mbPlayerIsInLobby     = false;
    meIsRanked            = E_GAME_RANKED_UNKNOWN;     // X360 li 2 -> meIsRanked(+0x2C)
    meOldRankedStatus     = E_GAME_RANKED_UNKNOWN;     // X360 li 2 -> meOldRankedStatus(+0x30)
    meOldDistrict         = static_cast<BrnWorld::EDistrict>(0);
    mePendingDistrict     = static_cast<BrnWorld::EDistrict>(0);

    CGS_ASSERT(mpGameStateModule, "Did you forget to construct the Rich Presence Manager.");
    CGS_ASSERT(mpNetworkRoundManager, "Did you forget to construct the Rich Presence Manager.");
    CGS_ASSERT(mpModeManager, "Did you forget to construct the Rich Presence Manager.");
    CGS_ASSERT(mpScoringSystem, "Did you forget to construct the Rich Presence Manager.");

    return true;
}

// X360 0x8237EFE8. Per-game-mode dispatch over meOldRichPresenceStatus (the cached presence state):
//   OFFLINE_RACE        -> position only
//   ONLINE_BURNING_HOME_RUN -> ranked, then round
//   ONLINE_RACE         -> ranked, then position, then round
// Returns true if any of the pushed parameters changed. The short-circuit structure (ranked-or-position
// first, then round) matches the X360 boolean fold.
bool RichPresenceManagerBase::SetGameTypeParameters()
{
    switch (meOldRichPresenceStatus)
    {
        case E_PRESENCE_STATE_OFFLINE_RACE:
            return SetPositionParameter();

        case E_PRESENCE_STATE_ONLINE_BURNING_HOME_RUN:
        {
            if (SetRankedParameter())
            {
                return true;
            }
            return SetRoundParameter();
        }

        case E_PRESENCE_STATE_ONLINE_RACE:
        {
            bool lbChanged = false;
            if (SetRankedParameter() || SetPositionParameter())
            {
                lbChanged = true;
            }
            if (lbChanged)
            {
                return true;
            }
            return SetRoundParameter();
        }

        default:
            return false;
    }
}

// X360 0x82372898. Push the player's current race position if it changed. Only does so while the
// scoring system has active cars; resolves the player's active-race-car slot through the game-state
// module and looks the position up in the scoring system. Returns true if the cached position changed.
bool RichPresenceManagerBase::SetPositionParameter()
{
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
    CGS_ASSERT(mpGameStateModule, "mpGameStateModule");

    if (mpScoringSystem->GetNumberOfActiveCars() == 0)
    {
        return false;
    }

    const EActiveRaceCarIndex leRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();
    const s32 liPosition = static_cast<s32>(mpScoringSystem->GetCarRacePosition(leRaceCarIndex));
    if (liPosition == miOldPosition)
    {
        return false;
    }

    miOldPosition = liPosition;
    SetCurrentPosition(liPosition);
    return true;
}

// X360 0x8235A6C8. Push the current round and total round count if either changed. The X360 reads the
// network round manager's raw counters: liTotalRounds = NRM.miTotalRounds (NRM+300) and the 0-based
// current round = NRM.miTotalRounds - NRM.miRoundsRemaining - 1 (NRM+300 - NRM+296 - 1). Those are the
// NetworkRoundManager public accessors GetTotalRounds() and GetCurrentRound() (the 0-based current-round
// index -- the X360 stores it verbatim into miCurrentRound and displays it +1). Both fields are cached
// so each is pushed only on a change; returns true if either was pushed.
bool RichPresenceManagerBase::SetRoundParameter()
{
    CGS_ASSERT(mpNetworkRoundManager, "mpNetworkRoundManager");

    bool lbChanged = false;

    const s32 liTotalRounds  = mpNetworkRoundManager->GetTotalRounds();
    const s32 liCurrentRound = mpNetworkRoundManager->GetCurrentRound();

    if (liCurrentRound != miCurrentRound)
    {
        miCurrentRound = liCurrentRound;
        SetCurrentRound(liCurrentRound + 1);
        lbChanged = true;
    }

    if (liTotalRounds != miTotalRounds)
    {
        miTotalRounds = liTotalRounds;
        SetTotalRounds(liTotalRounds);
        return true;
    }

    return lbChanged;
}

// X360 0x8235A798. Push the ranked/unranked status if it changed since last frame. Returns true if
// the status was pushed.
bool RichPresenceManagerBase::SetRankedParameter()
{
    CGS_ASSERT(mpNetworkRoundManager, "mpNetworkRoundManager");

    if (meIsRanked != meOldRankedStatus)
    {
        meOldRankedStatus = meIsRanked;
        SetRankedStatus(meIsRanked);
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// X360 0x8235A820 (class:BrnGameState catch-all TU, ledger "BrnGameState::RichPresenceManagerBase:").
// EGameModeTypeToERichPresenceState -- map an EGameModeType to the rich-presence state the platform
// presence layer displays, via the file-scope KE_GAME_MODES_TO_RICH_PRESENCE_STATES[] lookup table
// (X360 dword_8202AF20). The asm bounds-asserts the mode against [E_MODE_NONE, the invalid-lobby
// sentinel] (BrnGameStateRichPresenceManagerBase.cpp:458/459), returns E_PRESENCE_STATE_OFFLINE_DEFAULT
// for the two endpoint sentinels (E_MODE_NONE == -1 and the +18 invalid-lobby sentinel -- both mean
// "no real mode -> default presence"), and otherwise indexes the table by the mode ordinal.
//
// FLAG (data dependency, NOT fabricated): the 18 table entries live in this source's .rodata at X360
// 0x8202AF20 and are NOT recoverable from the function export. The table is declared extern here and
// DEFINED BY ITS OWNING DATA TU (the header note pins it to this source but defers its definition to
// avoid a duplicate); the control flow + bounds + sentinel handling above are byte-faithful to the asm.
// Compiles under the /c gate; a linker build needs the table TU to land. Do NOT invent the 18 values.
extern const RichPresenceManagerBase::ERichPresenceStates
    KE_GAME_MODES_TO_RICH_PRESENCE_STATES[GameStateModuleIO::E_MODE_COUNT + 1];

RichPresenceManagerBase::ERichPresenceStates
RichPresenceManagerBase::EGameModeTypeToERichPresenceState(GameStateModuleIO::EGameModeType leGameMode)
{
    CGS_ASSERT(leGameMode >= GameStateModuleIO::E_MODE_NONE,
               "leModeType >= GameStateModuleIO::E_MODE_NONE");
    CGS_ASSERT(static_cast<s32>(leGameMode) <= KI_INVALID_LOBBY_TYPE,
               "leModeType <= GameStateModuleIO::E_MODE_COUNT");

    if (leGameMode == GameStateModuleIO::E_MODE_NONE ||
        static_cast<s32>(leGameMode) == KI_INVALID_LOBBY_TYPE)
    {
        return E_PRESENCE_STATE_OFFLINE_DEFAULT;
    }

    return KE_GAME_MODES_TO_RICH_PRESENCE_STATES[static_cast<s32>(leGameMode)];
}

}
