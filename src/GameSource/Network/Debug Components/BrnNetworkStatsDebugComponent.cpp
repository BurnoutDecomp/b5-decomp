// Bodies for the network player-stats debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct   @ 0x825851E8   Prepare    @ 0x82585230   Release    @ 0x82593A28   Destruct @ 0x825852C0
//   GetName     @ 0x82585300   GetPath    @ (DWARF :200)  OnActivate @ 0x82593990
//   AddStats    @ 0x82585310   RemoveStats@ 0x82585398
//   GetELOFromServer @ 0x82591468   SetELOOnServer @ 0x82591560
//   _GetEloCallbackFunction @ 0x82585408   _SetEloCallbackFunction @ 0x82585548
//
// The component exposes the local player's three ELO values (Race / Burning Home Run / Road Rage)
// as editable debug-menu variables plus two menu actions that round-trip them through the server-
// interface custom-commands component (GetEloOfLocalPlayer / SetEloOfLocalPlayer); while a request
// is in flight the three variables are marked read-only, and the completion callbacks restore them
// (and, for the GET, also re-arm their step). AddStats / RemoveStats register and unregister a
// per-player group of the 12 NetworkPlayerStats values (one menu variable each), grouped under the
// player's name.

#include "GameSource/Network/Debug Components/BrnNetworkStatsDebugComponent.h"

#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"            // NetworkPlayerStats (12-value record)
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h" // ServerInterfaceCustomCommands::Get/SetEloOfLocalPlayer
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"             // GetCustomCommandsComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // EStatus
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

namespace BrnNetwork
{
    namespace
    {
        // Per-stat debug-menu labels, indexed by NetworkPlayerStats::EStatsValue. Only index 0 is
        // attested by the X360 asm (the file-scope name table `off_82F29F5C` whose first slot is
        // "Total Events"); the remaining slots' string bytes are not in the available exports, so
        // those labels are derived from the DecFIGS EStatsValue enum constant names (same documented
        // approach as the sibling BrnNetworkServerInterfaceDebugComponent option labels). The table
        // shape (12 entries, one per EStatsValue) and its first entry ARE attested.
        const char* const KPC_STAT_NAMES[NetworkPlayerStats::E_STATS_VALUE_COUNT] =
        {
            "Total Events",              // E_STATS_VALUE_TOTAL_GAMES_COMPLETED (asm-attested)
            "Ranked Games Completed",    // E_STATS_VALUE_RANKED_GAMES_COMPLETED
            "Unranked Games Completed",  // E_STATS_VALUE_UNRANKED_GAMES_COMPLETED
            "Win Percent",               // E_STATS_VALUE_WIN_PERCENT
            "Takedowns",                 // E_STATS_VALUE_TAKEDOWNS
            "Number Of Rivals",          // E_STATS_VALUE_NUMBER_OF_RIVALS
            "Achievements Earnt",        // E_STATS_VALUE_ACHIEVEMENTS_EARNT
            "Challenges Completed",      // E_STATS_VALUE_CHALLENGES_COMPLETED
            "Rank",                      // E_STATS_VALUE_RANK
            "Ranked Games Started",      // E_STATS_VALUE_RANKED_GAMES_STARTED
            "Unranked Games Started",    // E_STATS_VALUE_UNRANKED_GAMES_STARTED
            "Wins",                      // E_STATS_VALUE_WINS
        };

        // The custom-commands component for the local player's ELO get/set requests. The X360 reaches
        // it as `serverInterface->GetCustomCommandsComponent()` (the component slot at +0x4C) and
        // operates on the Brn leaf type; the accessor hands back the shared base, which is
        // reinterpret_cast to the leaf here (the documented component-accessor idiom).
        ServerInterfaceCustomCommands* GetCustomCommands(CgsNetwork::ServerInterface* lpServerInterface)
        {
            return reinterpret_cast<ServerInterfaceCustomCommands*>(
                lpServerInterface->GetCustomCommandsComponent());
        }
    }

    // @ 0x825851E8. Two-phase construct: run the base teardown/reset then register with the debug
    // manager and clear the owned members. The X360 emits a `bl` to an empty (blr-only) folded
    // function here (decompiled as BaseCollisionGenerator::Destruct via COMDAT folding of identical
    // empty bodies); it is a no-op and is dropped.
    void StatsDebugComponent::Construct()
    {
        Register();

        mpServerInterface   = nullptr;
        miRaceElo           = 0;
        miBurningHomeRunElo = 0;
        miRoadRageElo       = 0;
    }

    // @ 0x82585230. Bind the component to its server interface + stats manager and clear the ELO
    // values. Always returns true.
    bool StatsDebugComponent::Prepare(CgsNetwork::ServerInterface* lpServerInterface,
                                      NetworkPlayerStatsManager* lpStatsManager)
    {
        CGS_ASSERT(lpServerInterface != nullptr, "lpServerInterface");
        mpServerInterface = lpServerInterface;

        CGS_ASSERT(lpStatsManager != nullptr, "lpStatsManager");
        mpStatsManager = lpStatsManager;

        miRaceElo           = 0;
        miBurningHomeRunElo = 0;
        miRoadRageElo       = 0;
        return true;
    }

    // @ 0x82593A28. Unregister the ELO variables + the two menu actions, then clear the members.
    // Always returns true.
    bool StatsDebugComponent::Release()
    {
        UnregisterVariable(&miRaceElo);
        UnregisterVariable(&miRoadRageElo);
        UnregisterVariable(&miBurningHomeRunElo);
        UnregisterFunction(&StatsDebugComponent::GetELOFromServer, nullptr);
        UnregisterFunction(&StatsDebugComponent::SetELOOnServer, nullptr);

        mpServerInterface   = nullptr;
        miRaceElo           = 0;
        miBurningHomeRunElo = 0;
        miRoadRageElo       = 0;
        return true;
    }

    // @ 0x825852C0. Clear the owned members. The X360 first `bl`s an empty (blr-only) folded
    // function (decompiled as BaseCollisionGenerator::Destruct via COMDAT folding of identical empty
    // bodies) -- a no-op, dropped -- then stores 0 to mpServerInterface and the three ELO values.
    void StatsDebugComponent::Destruct()
    {
        mpServerInterface   = nullptr;
        miRaceElo           = 0;
        miBurningHomeRunElo = 0;
        miRoadRageElo       = 0;
    }

    // @ 0x82585300.
    const char* StatsDebugComponent::GetName() const
    {
        return "Stats";
    }

    // @ DWARF :200 (no distinct X360 body / folded). The component's menu path leaf.
    const char* StatsDebugComponent::GetPath() const
    {
        // FLAGGED: "Network" is a derived/unverified label -- GetPath has no X360 asm
        // body (folded) and no aNetwork string reference in this TU's exports.
        return "Network";
    }

    // @ 0x82593990. Register the menu surface when activated: the two ELO round-trip actions and the
    // three editable ELO variables.
    void StatsDebugComponent::OnActivate()
    {
        RegisterFunction(&StatsDebugComponent::GetELOFromServer, this, "Get ELO From Server");
        RegisterFunction(&StatsDebugComponent::SetELOOnServer, this, "Set ELO On Server");

        RegisterVariable(&miRaceElo, "Race Elo");
        RegisterVariable(&miRoadRageElo, "Road Rage Elo");
        RegisterVariable(&miBurningHomeRunElo, "Burning Home Run Elo");
    }

    // @ 0x82585310. Register one menu variable per stat value, grouped under the player's name. The
    // X360 asserts the loop index never overruns the 12-entry stat enum.
    void StatsDebugComponent::AddStats(NetworkPlayerStats* lpStats)
    {
        for (s32 liEnumIndex = NetworkPlayerStats::E_STATS_VALUE_START;
             liEnumIndex < NetworkPlayerStats::E_STATS_VALUE_COUNT;
             ++liEnumIndex)
        {
            RegisterVariable(lpStats->GetStatValuePtr(liEnumIndex),
                             lpStats->GetName(),
                             KPC_STAT_NAMES[liEnumIndex]);
            CGS_ASSERT(liEnumIndex <= NetworkPlayerStats::E_STATS_VALUE_COUNT,
                       "leEnumIndex <= NetworkPlayerStats::E_STATS_VALUE_COUNT");
        }
    }

    // @ 0x82585398. Unregister the per-stat menu variables added by AddStats.
    void StatsDebugComponent::RemoveStats(NetworkPlayerStats* lpStats)
    {
        for (s32 liEnumIndex = NetworkPlayerStats::E_STATS_VALUE_START;
             liEnumIndex < NetworkPlayerStats::E_STATS_VALUE_COUNT;
             ++liEnumIndex)
        {
            UnregisterVariable(lpStats->GetStatValuePtr(liEnumIndex));
            CGS_ASSERT(liEnumIndex <= NetworkPlayerStats::E_STATS_VALUE_COUNT,
                       "leEnumIndex <= NetworkPlayerStats::E_STATS_VALUE_COUNT");
        }
    }

    // @ 0x82591468. "Get ELO From Server" menu action: lock the three ELO variables read-only and
    // fire a GetEloOfLocalPlayer request, restored by _GetEloCallbackFunction on completion. The
    // X360 reads the source order (race, BURNING-HOME-RUN, road-rage) -- see the custom-commands
    // header note; mirrored here.
    void StatsDebugComponent::GetELOFromServer(void* lpData)
    {
        CGS_ASSERT(lpData != nullptr, "lpData");
        StatsDebugComponent* lpStatsDebugComponent = static_cast<StatsDebugComponent*>(lpData);

        CGS_ASSERT(lpStatsDebugComponent->mpServerInterface != nullptr,
                   "lpStatsDebugComponent->mpServerInterface");
        ServerInterfaceCustomCommands* lpCustomCommandsComponent =
            GetCustomCommands(lpStatsDebugComponent->mpServerInterface);
        CGS_ASSERT(lpCustomCommandsComponent != nullptr, "lpCustomCommandsComponent");

        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRaceElo, true);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRoadRageElo, true);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miBurningHomeRunElo, true);

        lpCustomCommandsComponent->GetEloOfLocalPlayer(
            &lpStatsDebugComponent->miRaceElo,
            &lpStatsDebugComponent->miBurningHomeRunElo,
            &lpStatsDebugComponent->miRoadRageElo,
            &StatsDebugComponent::_GetEloCallbackFunction,
            lpStatsDebugComponent);
    }

    // @ 0x82591560. "Set ELO On Server" menu action: lock the three ELO variables read-only and fire
    // a SetEloOfLocalPlayer request (source order race, road-rage, burning-home-run -- the reverse
    // of the GET order, per the custom-commands header note), restored by _SetEloCallbackFunction.
    void StatsDebugComponent::SetELOOnServer(void* lpData)
    {
        CGS_ASSERT(lpData != nullptr, "lpData");
        StatsDebugComponent* lpStatsDebugComponent = static_cast<StatsDebugComponent*>(lpData);

        CGS_ASSERT(lpStatsDebugComponent->mpServerInterface != nullptr,
                   "lpStatsDebugComponent->mpServerInterface");
        ServerInterfaceCustomCommands* lpCustomCommandsComponent =
            GetCustomCommands(lpStatsDebugComponent->mpServerInterface);
        CGS_ASSERT(lpCustomCommandsComponent != nullptr, "lpCustomCommandsComponent");

        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRaceElo, true);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miBurningHomeRunElo, true);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRoadRageElo, true);

        lpCustomCommandsComponent->SetEloOfLocalPlayer(
            lpStatsDebugComponent->miRaceElo,
            lpStatsDebugComponent->miRoadRageElo,
            lpStatsDebugComponent->miBurningHomeRunElo,
            &StatsDebugComponent::_SetEloCallbackFunction,
            lpStatsDebugComponent);
    }

    // @ 0x82585408. GetEloOfLocalPlayer completion: log, re-arm each ELO variable's step (100), drop
    // the read-only locks, and on failure clear the custom-commands component's error status.
    void StatsDebugComponent::_GetEloCallbackFunction(void* lpData, void* /*lpResult*/, bool lbSuccess)
    {
        CGS_ASSERT(lpData != nullptr, "lpData");
        StatsDebugComponent* lpStatsDebugComponent = static_cast<StatsDebugComponent*>(lpData);
        CGS_ASSERT(lpStatsDebugComponent != nullptr, "lpStatsDebugComponent");

        *CgsDev::Log::gpDebugPrint << "Got stat debug get callback\n";

        lpStatsDebugComponent->SetStep(&lpStatsDebugComponent->miRaceElo, 100);
        lpStatsDebugComponent->SetStep(&lpStatsDebugComponent->miRoadRageElo, 100);
        lpStatsDebugComponent->SetStep(&lpStatsDebugComponent->miBurningHomeRunElo, 100);

        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRaceElo, false);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRoadRageElo, false);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miBurningHomeRunElo, false);

        if (!lbSuccess)
        {
            CGS_ASSERT(lpStatsDebugComponent->mpServerInterface != nullptr,
                       "lpStatsDebugComponent->mpServerInterface");
            ServerInterfaceCustomCommands* lpCustomCommandsComponent =
                GetCustomCommands(lpStatsDebugComponent->mpServerInterface);
            if (lpCustomCommandsComponent->GetStatus() ==
                CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
            {
                lpCustomCommandsComponent->ClearLastError();
            }
        }
    }

    // @ 0x82585548. SetEloOfLocalPlayer completion: log, drop the read-only locks, and on failure
    // clear the custom-commands component's error status. (Unlike the GET callback it does not
    // re-arm the step.)
    void StatsDebugComponent::_SetEloCallbackFunction(void* lpData, void* /*lpResult*/, bool lbSuccess)
    {
        CGS_ASSERT(lpData != nullptr, "lpData");
        StatsDebugComponent* lpStatsDebugComponent = static_cast<StatsDebugComponent*>(lpData);
        CGS_ASSERT(lpStatsDebugComponent != nullptr, "lpStatsDebugComponent");

        *CgsDev::Log::gpDebugPrint << "Got stat debug set callback\n";

        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRaceElo, false);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miRoadRageElo, false);
        lpStatsDebugComponent->SetReadOnly(&lpStatsDebugComponent->miBurningHomeRunElo, false);

        if (!lbSuccess)
        {
            CGS_ASSERT(lpStatsDebugComponent->mpServerInterface != nullptr,
                       "lpStatsDebugComponent->mpServerInterface");
            ServerInterfaceCustomCommands* lpCustomCommandsComponent =
                GetCustomCommands(lpStatsDebugComponent->mpServerInterface);
            if (lpCustomCommandsComponent->GetStatus() ==
                CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
            {
                lpCustomCommandsComponent->ClearLastError();
            }
        }
    }
}
