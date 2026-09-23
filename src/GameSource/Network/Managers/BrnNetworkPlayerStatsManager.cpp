// ===================================================================================
// BrnNetwork::NetworkPlayerStatsManager  -- recovered function bodies
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkPlayerStatsManager.cpp
//
// This TU homes the three X360-recovered functions of the online player-stats manager:
//   NetworkPlayerStatsManager()              @ 0x827E1110  (the C++ constructor)
//   GetPlayerStatsByName(const char*)        @ 0x825526C8
//   HandleOfflineProgressionEvent(...)       @ 0x82546BB0  (private Process*Simulation helper)
//
// Every store / branch / constant below is grounded in the X360 assembly listed in the
// dossier postmortem (scratchpad/wave5). Sub-object layout/offsets are by-name only; the
// absolute X360 byte offsets are quoted as provenance comments.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"

#include <cstring>  // std::memcpy

#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameSource/Network/BrnServerInterface.h"                              // BrnServerInterface::GetStatus / EStatus
#include "GameSource/Network/BrnServerInterfaceBase.h"                          // GetCustomCommandsComponent()
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"     // ServerInterfaceCustomCommands::UploadOfflineProgress
#include "GameSource/Network/BrnNetworkModule.h"                                // BrnNetworkModule::GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkModuleIO.h"                              // NetworkEventQueue
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                      // NetworkOutGetOfflineProgression
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                // VariableEventQueue::AddEvent

namespace BrnNetwork
{
    // -----------------------------------------------------------------------------
    // NetworkPlayerStatsManager() @ 0x827E1110
    //
    // The compiler-synthesised C++ constructor. The X360 body is purely the inlined
    // construction of this object's embedded sub-objects:
    //   * each of the 7 StatsUpdateEntry's two StatsUpdateMessage members has its message
    //     vtable installed (the 14 `stw off_820CF30C` stores at +0x28C..+0x2A0);
    //   * the NetworkPlayerStatsResults cache (mPlayerStatsCache) has each of its 32
    //     NetworkPlayerStats records cleared (the 33-iteration loop zeroing each record's
    //     timestamp word + count, starting at +0x5D8);
    //   * the OnlineGameResults buffer (mBufferedOnlineStats) has its GameAction vtable
    //     installed (the `stw off_820CE3E4` at +0x1088 off the cache base) and its leading
    //     result words zeroed.
    //
    // The manager's OWN scalar/pointer/flag members (meCurrentStatus, the mpNetwork* pointers,
    // the mbWaiting* flags, miCachedNumberOfChallegesCompleted) are NOT touched by this ctor in
    // the asm -- they are initialised later by Construct(). So the human constructor simply lets
    // each embedded sub-object construct; the vtable installs + cache clear are owned by those
    // sub-objects' own constructors (StatsUpdateMessage / NetworkPlayerStatsResults /
    // OnlineGameResults), exactly as the binary inlines them here.
    // -----------------------------------------------------------------------------
    NetworkPlayerStatsManager::NetworkPlayerStatsManager()
    {
        // (no explicit body: the embedded sub-objects construct themselves, matching the
        //  binary's inlined sub-object construction; the manager's own scalars are set by
        //  Construct(), not by this ctor.)
    }

    // -----------------------------------------------------------------------------
    // GetPlayerStatsByName(const char* lpcName) @ 0x825526C8
    //
    // Assert the name is non-null, then forward to the cache's by-name lookup. The X360
    // tail-calls NetworkPlayerStatsResults::GetPlayerStats on this object's mPlayerStatsCache
    // (asm `addi r3, r30, 0x578`). The cache returns a mutable NetworkPlayerStats*; this
    // accessor hands it back const.
    // -----------------------------------------------------------------------------
    const NetworkPlayerStats* NetworkPlayerStatsManager::GetPlayerStatsByName(const char* lpcName)
    {
        CGS_ASSERT(lpcName != nullptr, "lpcName");

        return mPlayerStatsCache.GetPlayerStats(lpcName);
    }

    // -----------------------------------------------------------------------------
    // HandleOfflineProgressionEvent(const NetworkInOfflineProgression* lpEvent) @ 0x82546BB0
    //
    // Cache the incoming event's freeburn-challenge success count, then decide what to do with
    // the offline-progression record:
    //   * If we are armed for an offline-progression upload from the gamestate
    //     (mbWaitingForOfflineProgressFromGamestate), AND the manager is idle
    //     (meCurrentStatus == E_STATS_STATUS_IDLE), AND the server-interface's custom-commands
    //     component reports idle (GetStatus(E_COMPONENTS_CUSTOM_COMMANDS) == E_STATUS_IDLE):
    //       go to E_STATS_STATUS_OFFLINE_PROGRESSION and upload the record now, clearing both
    //       the armed flag and the pending-upload flag.
    //   * Otherwise: buffer the whole event for a later upload and raise the pending-upload flag.
    //
    // X360 offsets (provenance): mpServerInterface @+0x1774, meCurrentStatus @+0x177C,
    // miCachedNumberOfChallegesCompleted @+0x1780, mbWaitingToUploadOfflineProgress @+0x1785,
    // mbWaitingForOfflineProgressFromGamestate @+0x1786, mBufferedOfflineProgression @+0x1728.
    // The component status code 9 == CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS; the idle status
    // value 2 == BrnServerInterface::E_STATUS_IDLE; the new status 7 ==
    // E_STATS_STATUS_OFFLINE_PROGRESSION (all read from the asm immediates).
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::HandleOfflineProgressionEvent(
        const BrnNetworkModuleIO::NetworkInOfflineProgression* lpEvent)
    {
        // Always cache the event's freeburn-challenge success count (asm: store of *(a2+0x40)
        // into +0x1780, unconditional).
        miCachedNumberOfChallegesCompleted = lpEvent->miFreeburnChallengeSuccessCount;

        if (!mbWaitingForOfflineProgressFromGamestate)
            return;

        if (meCurrentStatus == E_STATS_STATUS_IDLE
            && mpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS)
                   == BrnServerInterface::E_STATUS_IDLE)
        {
            meCurrentStatus = E_STATS_STATUS_OFFLINE_PROGRESSION;

            CGS_ASSERT(mpServerInterface != nullptr, "mpServerInterface");
            CGS_ASSERT(mpServerInterface->GetCustomCommandsComponent() != nullptr,
                       "mpServerInterface->GetCustomCommandsComponent()");

            // The event's OfflineProgressionT record sits at offset 0, so the event pointer
            // doubles as the OFFPROG record pointer the custom-commands component expects.
            mpServerInterface->GetCustomCommandsComponent()->UploadOfflineProgress(
                &lpEvent->mOfflineProgression, miCachedNumberOfChallegesCompleted);

            mbWaitingForOfflineProgressFromGamestate = false;
            mbWaitingToUploadOfflineProgress         = false;
        }
        else
        {
            // Buffer the whole 68-byte event for a later upload attempt (asm: memcpy of 0x44
            // bytes from a2 into +0x1728), and raise the pending-upload flag.
            std::memcpy(&mBufferedOfflineProgression, lpEvent, sizeof(mBufferedOfflineProgression));
            mbWaitingToUploadOfflineProgress = true;
        }
    }

    // -----------------------------------------------------------------------------
    // UploadOfflineProgression()
    //
    // Ask the game state for the offline progression record (the one-byte request event, tag
    // 55) and arm the wait for it; HandleOfflineProgressionEvent uploads it when it arrives.
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::UploadOfflineProgression()
    {
        BrnNetworkModuleIO::NetworkOutGetOfflineProgression lRequestEvent;
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequestEvent),
            lRequestEvent.GetEventType(), sizeof(lRequestEvent));
        mbWaitingForOfflineProgressFromGamestate = true;
    }

    // -----------------------------------------------------------------------------
    // HandleFreeburnChallengeEvent(const NetworkInFreeburnChallengeEvent*)
    //
    // A freeburn challenge was completed: remember the new completed-challenge count and
    // publish it as the local player's challenges-completed stat. (Inlined into the state
    // manager's network-event pump on the console.)
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::HandleFreeburnChallengeEvent(
        const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent* lpEvent)
    {
        if (lpEvent->meChallengeStatus == BrnGameState::E_CHALLENGE_STATUS_SUCCESS)
        {
            miCachedNumberOfChallegesCompleted = lpEvent->miNumberOfCompletedChallenges;
            UpdateLocalPlayersStat(NetworkPlayerStats::E_STATS_VALUE_CHALLENGES_COMPLETED,
                                   lpEvent->miNumberOfCompletedChallenges);
        }
    }

    // -----------------------------------------------------------------------------
    // HandleNewNumberOfAchievements(s32)
    //
    // Publish the new achievement count as the local player's achievements stat. (Inlined into
    // the state manager's game-state action pump on the console.)
    // -----------------------------------------------------------------------------
    void NetworkPlayerStatsManager::HandleNewNumberOfAchievements(s32 liNumberOfAchievements)
    {
        UpdateLocalPlayersStat(NetworkPlayerStats::E_STATS_VALUE_ACHIEVEMENTS_EARNT, liNumberOfAchievements);
    }
}
