#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"

// ===================================================================================
// CgsNetwork::HostMigrationManager -- peer-to-peer host migration.
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsHostMigrationManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ARTIST pseudocode/asm is the spine;
// DecFIGS DWARF gives the declaration shape; Feb-2007 partial has nothing for this TU).
//
// The decompiler inlined every PlayerManager registry walk into raw record-table offset
// arithmetic (+0x2378 record stride, +0x2398 host id, +0x239C local id, ...). Those are
// de-inlined here back into the PlayerManager API the X360 source actually called:
//   GetNextPlayerID / GetPlayerByID / GetNextLocalPlayerID / SetHostPlayerID.
// Every member of this class is accessed BY NAME against the frozen layout in the header.
//
// IsHostAlive lives in CgsMessageSubclasses.cpp (its own ledger TU); it is only called
// here. NewHostMessage / HostKeepAliveMessage (de)serialise via their own TUs.
// ===================================================================================

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"   // UInt16IsLargerWrapped, flag accessors
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // qsort
#include <cstring>   // memmove

namespace CgsSystem { class TimerStatus; }

namespace CgsNetwork
{
    // -------------------------------------------------------------------------------
    // Frame-rate-keyed keep-alive tunables (DWARF CgsHostMigrationManager.cpp:30-36).
    // Values are in network frames; the 50Hz and 60Hz sets scale identically in seconds.
    static const u16 KU16_HOSTKEEPALIVE_SEND_TIMEOUT50    = 20;
    static const u16 KU16_HOSTKEEPALIVE_TIMEOUT50         = 150;
    static const u16 KU16_HOSTKEEPALIVE_INITIAL_TIMEOUT50 = 500;
    static const u16 KU16_HOSTKEEPALIVE_SEND_TIMEOUT60    = 24;
    static const u16 KU16_HOSTKEEPALIVE_TIMEOUT60         = 180;
    static const u16 KU16_HOSTKEEPALIVE_INITIAL_TIMEOUT60 = 600;

    // The "no player" sentinel (CgsNetwork::KI_INVALID_PLAYER_ID from CgsNetworkPlayer.h) and
    // the "no frame" sentinel (CgsNetwork::KU16_INVALID_FRAME from CgsMessage.h) are reused by
    // name -- they are already declared in the included headers.

    // Registered message-type ids for the two host-migration messages (per RegisterMessages).
    static const s32 KI_MESSAGE_TYPE_HOST_KEEP_ALIVE = 8;
    static const s32 KI_MESSAGE_TYPE_NEW_HOST        = 9;

    // QSortCallback runs through the C qsort with no user-data channel, so the comparator's
    // reference host id is published through this file-scope global (set/cleared around the
    // qsort call in Enable). DWARF CgsHostMigrationManager.cpp:49.
    static HostMigrationManager::NetworkPlayerID gQuickSortHostPlayerID = KI_INVALID_PLAYER_ID;

    // -------------------------------------------------------------------------------
    void HostMigrationManager::Construct()
    {
        mHostPlayerID                     = KI_INVALID_PLAYER_ID;
        mu16LastHostKeepAliveSendTime     = KU16_INVALID_FRAME;
        mu16LastHostKeepAliveReceivedTime = KU16_INVALID_FRAME;
        mu16HostKeepAliveSendTimeout      = KU16_INVALID_FRAME;
        mu16HostKeepAliveTimeout          = KU16_INVALID_FRAME;
        mu16HostKeepAliveInitialTimeout   = KU16_INVALID_FRAME;
        mpPlayerManager                   = nullptr;
        meFrameRate                       = static_cast<CgsSystem::EFrameRate>(-1);

        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
            maHostMigrationPlayerIDs[liIndex] = KI_INVALID_PLAYER_ID;

        for (s32 liIndex = 0; liIndex < KI_MAX_HOST_MIGRATION_CALLBACKS; ++liIndex)
        {
            mapHostMigrationCallback[liIndex]     = nullptr;
            mapHostMigrationCallbackData[liIndex] = nullptr;
        }

        mHostMigrationDebugComponent.mpHostMigrationManager = this;
        mHostMigrationDebugComponent.mLocalPlayerID         = KI_INVALID_PLAYER_ID;
        mHostMigrationDebugComponent.Register();
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::SetFrameRate(CgsSystem::EFrameRate leFrameRate)
    {
        meFrameRate = leFrameRate;

        switch (leFrameRate)
        {
        case static_cast<CgsSystem::EFrameRate>(-1):
            mu16HostKeepAliveSendTimeout    = KU16_INVALID_FRAME;
            mu16HostKeepAliveTimeout        = KU16_INVALID_FRAME;
            mu16HostKeepAliveInitialTimeout = KU16_INVALID_FRAME;
            break;

        case CgsSystem::E_FRAMERATE_50HZ:
            mu16HostKeepAliveSendTimeout    = KU16_HOSTKEEPALIVE_SEND_TIMEOUT50;
            mu16HostKeepAliveTimeout        = KU16_HOSTKEEPALIVE_TIMEOUT50;
            mu16HostKeepAliveInitialTimeout = KU16_HOSTKEEPALIVE_INITIAL_TIMEOUT50;
            break;

        case CgsSystem::E_FRAMERATE_60HZ:
            mu16HostKeepAliveSendTimeout    = KU16_HOSTKEEPALIVE_SEND_TIMEOUT60;
            mu16HostKeepAliveTimeout        = KU16_HOSTKEEPALIVE_TIMEOUT60;
            mu16HostKeepAliveInitialTimeout = KU16_HOSTKEEPALIVE_INITIAL_TIMEOUT60;
            break;

        default:
            *CgsDev::Log::gpDebugPrint << "Invalid frame rate " << static_cast<s32>(meFrameRate);
            CGS_ASSERT(false, "Invalid frame rate");
            break;
        }
    }

    // -------------------------------------------------------------------------------
    bool HostMigrationManager::Prepare(PlayerManager* lpPlayerManager,
                                       CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                       u16 lu16CurrentFrame)
    {
        mpPlayerManager = lpPlayerManager;
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
            maHostMigrationPlayerIDs[liIndex] = KI_INVALID_PLAYER_ID;

        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_NETWORK_PLAYERS; ++liPlayerIndex)
            maMessageData[liPlayerIndex].mOwnerPlayer = KI_INVALID_PLAYER_ID;

        mHostPlayerID                     = KI_INVALID_PLAYER_ID;
        mu16LastHostKeepAliveSendTime     = KU16_INVALID_FRAME;
        mu16LastHostKeepAliveReceivedTime = KU16_INVALID_FRAME;
        mu16HostKeepAliveSendTimeout      = KU16_INVALID_FRAME;
        mu16HostKeepAliveTimeout          = KU16_INVALID_FRAME;
        mu16HostKeepAliveInitialTimeout   = KU16_INVALID_FRAME;

        SetFrameRate(leLocalConsoleFrameRate);

        // Seed the "last received" frame an initial-timeout window into the future so a
        // freshly prepared session does not immediately fail the alive check.
        mu16LastHostKeepAliveReceivedTime =
            static_cast<u16>(mu16HostKeepAliveInitialTimeout + lu16CurrentFrame);
        if (mu16LastHostKeepAliveReceivedTime == KU16_INVALID_FRAME)
            mu16LastHostKeepAliveReceivedTime = 0;

        return true;
    }

    // -------------------------------------------------------------------------------
    bool HostMigrationManager::Release()
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        ClearAllCallbacks();

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            HostMigrationNetworkPlayerData& lData = maMessageData[liIndex];
            lData.mOwnerPlayer = KI_INVALID_PLAYER_ID;
            lData.mHostKeepAliveMessageSend.Construct();
            lData.mHostKeepAliveMessageRecv.Construct();
            lData.mNewHostMessageSend.Construct();
            lData.mNewHostMessageRecv.Construct();
        }

        mpPlayerManager                 = nullptr;
        meFrameRate                     = static_cast<CgsSystem::EFrameRate>(-1);
        mu16HostKeepAliveSendTimeout    = KU16_INVALID_FRAME;
        mu16HostKeepAliveTimeout        = KU16_INVALID_FRAME;
        mu16HostKeepAliveInitialTimeout = KU16_INVALID_FRAME;

        return true;
    }

    // ClearAllCallbacks @ CgsHostMigrationManager.cpp:1348 -- reachable only through Release
    // (the X360 build inlined it there). Clears the registered host-migration callback table.
    void HostMigrationManager::ClearAllCallbacks()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_HOST_MIGRATION_CALLBACKS; ++liIndex)
        {
            mapHostMigrationCallback[liIndex]     = nullptr;
            mapHostMigrationCallbackData[liIndex] = nullptr;
        }
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::Destruct()
    {
        // The X360 Destruct is empty (the manager owns no heap resources; Release tears the
        // session-scoped state down). Kept as a named no-op for the lifecycle symmetry.
    }

    // -------------------------------------------------------------------------------
    HostMigrationManager::NetworkPlayerID HostMigrationManager::GetHostPlayerID()
    {
        return mHostPlayerID;
    }

    // -------------------------------------------------------------------------------
    // Deterministic election comparator over maHostMigrationPlayerIDs. The current host
    // (gQuickSortHostPlayerID) always sorts first; otherwise lower id sorts first.
    s32 HostMigrationManager::QSortCallback(const void* lpData1, const void* lpData2)
    {
        const NetworkPlayerID liHostID = gQuickSortHostPlayerID;
        CGS_ASSERT(gQuickSortHostPlayerID >= 0, "gQuickSortHostPlayerID >= 0");

        const NetworkPlayerID liA = *static_cast<const NetworkPlayerID*>(lpData1);
        const NetworkPlayerID liB = *static_cast<const NetworkPlayerID*>(lpData2);

        if (liA == liHostID)
            return -1;
        if (liB == liHostID)
            return 1;
        if (liA < liB)
            return -1;
        return (liA > liB) ? 1 : 0;
    }

    // -------------------------------------------------------------------------------
    // Election rule: does player A become host before player B? A current-host id always wins;
    // otherwise the player earlier in maHostMigrationPlayerIDs (lower election index) wins.
    bool HostMigrationManager::ABecomesHostBeforeB(NetworkPlayerID liPlayerIDa,
                                                   NetworkPlayerID liPlayerIDb)
    {
        if (liPlayerIDb == KI_INVALID_PLAYER_ID)
            return true;
        if (liPlayerIDa == KI_INVALID_PLAYER_ID)
            return false;

        s32 liIndexA = -1;
        s32 liIndexB = -1;
        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            if (maHostMigrationPlayerIDs[liIndex] == liPlayerIDa)
                liIndexA = liIndex;
            if (maHostMigrationPlayerIDs[liIndex] == liPlayerIDb)
                liIndexB = liIndex;
        }

        return liIndexA < liIndexB;
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::Enable(NetworkPlayerID liGameRoomHostID,
                                      const CgsSystem::TimerStatus* lpTimerStatus,
                                      u16 lu16CurrentFrame)
    {
        SetNewHost(liGameRoomHostID, lpTimerStatus, lu16CurrentFrame);

        // Snapshot the current player set into the election list, then sort it.
        s32 liNumPlayers = 0;
        NetworkPlayerID liPlayerID = KI_INVALID_PLAYER_ID;
        if (mpPlayerManager->GetNextPlayerID(&liPlayerID,
                                             PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            do
            {
                maHostMigrationPlayerIDs[liNumPlayers] = liPlayerID;
                ++liNumPlayers;
            }
            while (mpPlayerManager->GetNextPlayerID(&liPlayerID,
                                                    PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED));
        }

        gQuickSortHostPlayerID = mHostPlayerID;
        qsort(maHostMigrationPlayerIDs, static_cast<size_t>(liNumPlayers),
              sizeof(NetworkPlayerID), &HostMigrationManager::QSortCallback);
        gQuickSortHostPlayerID = KI_INVALID_PLAYER_ID;

        for (s32 liIndex = liNumPlayers; liIndex < KI_MAX_PLAYERS; ++liIndex)
            maHostMigrationPlayerIDs[liIndex] = KI_INVALID_PLAYER_ID;
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::Disable(NetworkPlayerID liGameRoomHostID,
                                       const CgsSystem::TimerStatus* lpTimerStatus,
                                       u16 lu16CurrentFrame)
    {
        SetNewHost(liGameRoomHostID, lpTimerStatus, lu16CurrentFrame);
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::AddPlayer(NetworkPlayerID liPlayerID)
    {
        RegisterMessages(liPlayerID);
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::RemovePlayer(NetworkPlayerID liPlayerID)
    {
        CGS_ASSERT(liPlayerID != KI_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

        // Find the player in the election list and compact the gap.
        s32 liArrayIndex = 0;
        while (maHostMigrationPlayerIDs[liArrayIndex] != liPlayerID)
        {
            ++liArrayIndex;
            if (liArrayIndex >= KI_MAX_PLAYERS)
            {
                CGS_ASSERT(false, "liArrayIndex < KI_MAX_PLAYERS");
                return;
            }
        }

        if (liArrayIndex < KI_MAX_PLAYERS - 1)
        {
            const s32 liNumEntriesToMove = (KI_MAX_PLAYERS - 1) - liArrayIndex;
            CGS_ASSERT(liNumEntriesToMove > 0, "liNumEntriesToMove > 0");
            memmove(&maHostMigrationPlayerIDs[liArrayIndex],
                    &maHostMigrationPlayerIDs[liArrayIndex + 1],
                    sizeof(NetworkPlayerID) * static_cast<size_t>(liNumEntriesToMove));
        }

        if (mHostPlayerID == liPlayerID)
        {
            mHostPlayerID = KI_INVALID_PLAYER_ID;
            mpPlayerManager->SetHostPlayerID(KI_INVALID_PLAYER_ID);
        }
        maHostMigrationPlayerIDs[KI_MAX_PLAYERS - 1] = KI_INVALID_PLAYER_ID;

        // Only unregister the per-player messages for remote players (a local player keeps
        // its slot for the lifetime of the session).
        NetworkPlayerID liLocalPlayerID = KI_INVALID_PLAYER_ID;
        mpPlayerManager->GetNextLocalPlayerID(&liLocalPlayerID);
        if (liPlayerID != liLocalPlayerID)
            UnregisterMessages(liPlayerID);
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::Disconnected()
    {
        for (s32 liArrayIndex = KI_MAX_PLAYERS - 1; liArrayIndex >= 0; --liArrayIndex)
        {
            if (maHostMigrationPlayerIDs[liArrayIndex] != KI_INVALID_PLAYER_ID)
            {
                RemovePlayer(maHostMigrationPlayerIDs[liArrayIndex]);
                CGS_ASSERT(maHostMigrationPlayerIDs[liArrayIndex] == KI_INVALID_PLAYER_ID,
                           "maHostMigrationPlayerIDs[liArrayIndex] == K_INVALID_PLAYER_ID");
            }
        }
        mHostPlayerID = KI_INVALID_PLAYER_ID;
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::RegisterHostMigrationCallback(HostMigrationCallback lpfCallback,
                                                             void* lpUserData)
    {
        s32 liIndex = 0;
        while (mapHostMigrationCallback[liIndex] != nullptr)
        {
            ++liIndex;
            if (liIndex >= KI_MAX_HOST_MIGRATION_CALLBACKS)
            {
                CGS_ASSERT(false, "Failed to register host migration callback.");
                return;
            }
        }
        mapHostMigrationCallback[liIndex]     = lpfCallback;
        mapHostMigrationCallbackData[liIndex] = lpUserData;
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::UnregisterHostMigrationCallback(HostMigrationCallback lpfCallback)
    {
        s32 liIndex = 0;
        while (mapHostMigrationCallback[liIndex] != lpfCallback)
        {
            ++liIndex;
            if (liIndex >= KI_MAX_HOST_MIGRATION_CALLBACKS)
            {
                CGS_ASSERT(false, "Callback was not registered so hence could not be called.");
                return;
            }
        }
        mapHostMigrationCallback[liIndex]     = nullptr;
        mapHostMigrationCallbackData[liIndex] = nullptr;
    }

    // -------------------------------------------------------------------------------
    // Re-point the session host and fire the registered host-migration callbacks.
    void HostMigrationManager::SetNewHost(NetworkPlayerID liNewHostID,
                                          const CgsSystem::TimerStatus* lpTimerStatus,
                                          u16 lu16CurrentFrame)
    {
        const NetworkPlayerID liOldHostID = mHostPlayerID;

        mu16LastHostKeepAliveReceivedTime = lu16CurrentFrame;
        mHostPlayerID                     = liNewHostID;
        mu16LastHostKeepAliveSendTime     = KU16_INVALID_FRAME;
        mpPlayerManager->SetHostPlayerID(liNewHostID);

        for (s32 liCallbackIndex = 0; liCallbackIndex < KI_MAX_HOST_MIGRATION_CALLBACKS; ++liCallbackIndex)
        {
            if (mapHostMigrationCallback[liCallbackIndex] != nullptr)
            {
                mapHostMigrationCallback[liCallbackIndex](lpTimerStatus, liOldHostID, liNewHostID,
                                                          mapHostMigrationCallbackData[liCallbackIndex]);
            }
        }
    }

    // -------------------------------------------------------------------------------
    // Register the two host-migration messages (keep-alive + new-host) for a player slot.
    void HostMigrationManager::RegisterMessages(NetworkPlayerID liPlayerID)
    {
        *CgsDev::Log::gpDebugPrint << "CgsNetwork::HostMigrationManager::RegisterMessages"
                                   << ": id " << static_cast<s32>(liPlayerID) << "\n";

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            CGS_ASSERT(maMessageData[liIndex].mOwnerPlayer != liPlayerID,
                       "maMessageData[liIndex].mOwnerPlayer != lPlayerID");
        }

        s32 liFreeEntry = 0;
        while (maMessageData[liFreeEntry].mOwnerPlayer != KI_INVALID_PLAYER_ID)
        {
            ++liFreeEntry;
            if (liFreeEntry >= KI_MAX_NETWORK_PLAYERS)
            {
                CGS_ASSERT(false, "liFreeEntry<KI_MAX_NETWORK_PLAYERS");
                break;
            }
        }

        HostMigrationNetworkPlayerData& lData = maMessageData[liFreeEntry];
        lData.mOwnerPlayer = liPlayerID;
        lData.mHostKeepAliveMessageSend.Construct();
        lData.mHostKeepAliveMessageRecv.Construct();
        lData.mNewHostMessageSend.Construct();
        lData.mNewHostMessageRecv.Construct();

        NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
        CGS_ASSERT(lpNetPlayer != nullptr, "lpNetPlayer");

        lpNetPlayer->RegisterMessageType(KI_MESSAGE_TYPE_HOST_KEEP_ALIVE, 32,
                                         &lData.mHostKeepAliveMessageSend,
                                         &lData.mHostKeepAliveMessageRecv, nullptr, nullptr, nullptr);
        lpNetPlayer->RegisterMessageType(KI_MESSAGE_TYPE_NEW_HOST, 68,
                                         &lData.mNewHostMessageSend,
                                         &lData.mNewHostMessageRecv, nullptr, nullptr, nullptr);
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::UnregisterMessages(NetworkPlayerID liPlayerID)
    {
        NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
        CGS_ASSERT(lpNetPlayer != nullptr, "lpNetPlayer");

        s32 liEntry = 0;
        while (maMessageData[liEntry].mOwnerPlayer != liPlayerID)
        {
            ++liEntry;
            if (liEntry >= KI_MAX_NETWORK_PLAYERS)
            {
                CGS_ASSERT(false, "liEntry<KI_MAX_NETWORK_PLAYERS");
                return;
            }
        }

        if (lpNetPlayer != nullptr)
        {
            if (lpNetPlayer->IsMessageTypeRegistered(KI_MESSAGE_TYPE_HOST_KEEP_ALIVE))
                lpNetPlayer->UnRegisterMessageType(KI_MESSAGE_TYPE_HOST_KEEP_ALIVE);
            if (lpNetPlayer->IsMessageTypeRegistered(KI_MESSAGE_TYPE_NEW_HOST))
                lpNetPlayer->UnRegisterMessageType(KI_MESSAGE_TYPE_NEW_HOST);
        }

        maMessageData[liEntry].mOwnerPlayer = KI_INVALID_PLAYER_ID;
    }

    // -------------------------------------------------------------------------------
    void HostMigrationManager::SendHostKeepAliveMessage(NetworkPlayer* lpNetPlayer, u16 lu16CurrentFrame)
    {
        CGS_ASSERT(lpNetPlayer != nullptr, "lpNetPlayer");
        CGS_ASSERT(mHostPlayerID != KI_INVALID_PLAYER_ID, "mHostPlayerID != K_INVALID_PLAYER_ID");

        Message* lpMessage = lpNetPlayer->GetRegisteredSendMessage(KI_MESSAGE_TYPE_HOST_KEEP_ALIVE);
        static_cast<HostKeepAliveMessage*>(lpMessage)->PrepareForSend(lu16CurrentFrame);
    }

    // -------------------------------------------------------------------------------
    // Send the host keep-alive heartbeat to every player whose round-robin turn it is.
    void HostMigrationManager::UpdateSendingOfHostKeepAliveMessage(u16 lu16CurrentFrame)
    {
        if (mu16LastHostKeepAliveSendTime != KU16_INVALID_FRAME)
        {
            CGS_ASSERT(!UInt16IsLargerWrapped(mu16LastHostKeepAliveSendTime, lu16CurrentFrame),
                       "!UInt16IsLargerWrapped( mu16LastHostKeepAliveSendTime, lu16CurrentFrame )");
            if (UInt16IsLargerWrapped(mu16LastHostKeepAliveSendTime, lu16CurrentFrame))
                return;
        }

        CGS_ASSERT(lu16CurrentFrame != KU16_INVALID_FRAME, "lu16CurrentFrame != KU16_INVALID_FRAME");
        mu16LastHostKeepAliveSendTime = lu16CurrentFrame;

        NetworkPlayerID liPlayerID = KI_INVALID_PLAYER_ID;
        for (bool lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
             lbHasPlayer;
             lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
            if (lpNetPlayer != nullptr && !lpNetPlayer->HasConnectionFailed() &&
                mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(liPlayerID, true, 0))
            {
                SendHostKeepAliveMessage(lpNetPlayer, lu16CurrentFrame);
            }
        }
    }

    // -------------------------------------------------------------------------------
    // Walk every player's received host-migration messages and elect:
    //   lpBestHostKeepAlivePlayerID -- the best host according to received keep-alives, and
    //   lpBestNewHostPlayerID       -- the best new host according to received new-host votes,
    // accumulating that winner's received-clients list into lpaBestReceivedClientsIDs.
    void HostMigrationManager::FindBestHostIDs(NetworkPlayerID* lpBestHostKeepAlivePlayerID,
                                               NetworkPlayerID* lpBestNewHostPlayerID,
                                               NetworkPlayerID* lpaBestReceivedClientsIDs)
    {
        NetworkPlayerID liBestHostKeepAliveID = mHostPlayerID;
        NetworkPlayerID liBestNewHostID       = mHostPlayerID;

        NetworkPlayerID liPlayerID = KI_INVALID_PLAYER_ID;
        for (bool lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
             lbHasPlayer;
             lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
            if (lpNetPlayer == nullptr)
                continue;

            HostKeepAliveMessage* lpKeepAlive = static_cast<HostKeepAliveMessage*>(
                lpNetPlayer->GetRegisteredRecvMessage(KI_MESSAGE_TYPE_HOST_KEEP_ALIVE));
            NewHostMessage* lpNewHost = static_cast<NewHostMessage*>(
                lpNetPlayer->GetRegisteredRecvMessage(KI_MESSAGE_TYPE_NEW_HOST));

            // A received-and-valid keep-alive nominates this player's reported host.
            bool lbGotKeepAlive = false;
            if (lpKeepAlive->IsMessageValid())
            {
                lpKeepAlive->SetMessageInvalid();
                CGS_ASSERT(!lpKeepAlive->IsMessageValid(), "!IsMessageValid()");
                lbGotKeepAlive = true;
            }
            if (lbGotKeepAlive)
            {
                // The keep-alive nominates the SENDING player's own id (X360 reads
                // NetworkPlayer+0x008 = mPlayerID), not a field off the NewHost message.
                const NetworkPlayerID liFoundHostID = liPlayerID;
                CGS_ASSERT(liFoundHostID != KI_INVALID_PLAYER_ID, "lFoundHostID != K_INVALID_PLAYER_ID");
                if (ABecomesHostBeforeB(liBestHostKeepAliveID, liFoundHostID))
                    liBestHostKeepAliveID = liFoundHostID;
            }

            // A received new-host vote nominates its new host + received-clients set.
            NetworkPlayerID liReceivedNewHostID = KI_INVALID_PLAYER_ID;
            NetworkPlayerID laReceivedClientsIDs[KI_MAX_PLAYERS];
            if (lpNewHost->Retrieve(&liReceivedNewHostID, laReceivedClientsIDs))
            {
                for (s32 liCheck1 = 1; liCheck1 < KI_MAX_PLAYERS; ++liCheck1)
                {
                    for (s32 liCheck2 = liCheck1 + 1; liCheck2 < KI_MAX_PLAYERS; ++liCheck2)
                    {
                        CGS_ASSERT(laReceivedClientsIDs[liCheck2] == KI_INVALID_PLAYER_ID ||
                                       laReceivedClientsIDs[liCheck1] != laReceivedClientsIDs[liCheck2],
                                   "laReceivedClientsIDs[lnCheck2] == K_INVALID_PLAYER_ID || "
                                   "laReceivedClientsIDs[lnCheck1] != laReceivedClientsIDs[lnCheck2]");
                    }
                }

                if (ABecomesHostBeforeB(liBestNewHostID, liReceivedNewHostID))
                {
                    liBestNewHostID = liReceivedNewHostID;

                    // Copy the received-clients list into the running-best list, appending our
                    // own local player id, then pad the remainder with the invalid sentinel.
                    s32 liIndex2 = 0;
                    while (laReceivedClientsIDs[liIndex2] != KI_INVALID_PLAYER_ID)
                    {
                        lpaBestReceivedClientsIDs[liIndex2] = laReceivedClientsIDs[liIndex2];
                        ++liIndex2;
                        if (liIndex2 >= KI_MAX_PLAYERS)
                        {
                            CGS_ASSERT(false, "liIndex2 < KI_MAX_PLAYERS");
                            break;
                        }
                    }
                    lpaBestReceivedClientsIDs[liIndex2] = KI_INVALID_PLAYER_ID;

                    NetworkPlayerID liLocalPlayerID = KI_INVALID_PLAYER_ID;
                    const bool lbHasLocal = mpPlayerManager->GetNextLocalPlayerID(&liLocalPlayerID);
                    CGS_ASSERT(lbHasLocal, "mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID)");

                    lpaBestReceivedClientsIDs[liIndex2] = liLocalPlayerID;
                    for (s32 liPad = liIndex2 + 1; liPad < KI_MAX_PLAYERS; ++liPad)
                    {
                        CGS_ASSERT(laReceivedClientsIDs[liPad] == KI_INVALID_PLAYER_ID,
                                   "laReceivedClientsIDs[liIndex2] == K_INVALID_PLAYER_ID");
                        lpaBestReceivedClientsIDs[liPad] = KI_INVALID_PLAYER_ID;
                    }
                }
            }
        }

        *lpBestNewHostPlayerID       = liBestNewHostID;
        *lpBestHostKeepAlivePlayerID = liBestHostKeepAliveID;
        CGS_ASSERT(*lpBestNewHostPlayerID != KI_INVALID_PLAYER_ID,
                   "*lpBestNewHostPlayerID != K_INVALID_PLAYER_ID");
        CGS_ASSERT(*lpBestHostKeepAlivePlayerID != KI_INVALID_PLAYER_ID,
                   "*lpBestHostKeepAlivePlayerID != K_INVALID_PLAYER_ID");
    }

    // -------------------------------------------------------------------------------
    // If the elected new host differs from the current host, broadcast a NewHostMessage to
    // every player that is not already in the elected received-clients set.
    void HostMigrationManager::SendNewHostMessageIfRequired(u16 lu16CurrentFrame,
                                                            NetworkPlayerID liNewHostID,
                                                            NetworkPlayerID* lpaReceivedClientsIDs)
    {
        if (liNewHostID == mHostPlayerID)
            return;

        NetworkPlayerID liPlayerID = KI_INVALID_PLAYER_ID;
        for (bool lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
             lbHasPlayer;
             lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
            if (lpNetPlayer == nullptr)
                continue;

            // Skip players already in the received-clients set.
            s32 liIndex = 0;
            while (liIndex < KI_MAX_PLAYERS && lpaReceivedClientsIDs[liIndex] != liPlayerID)
                ++liIndex;
            if (liIndex != KI_MAX_PLAYERS)
                continue;

            for (s32 liCheck1 = 1; liCheck1 < KI_MAX_PLAYERS; ++liCheck1)
            {
                for (s32 liCheck2 = liCheck1 + 1; liCheck2 < KI_MAX_PLAYERS; ++liCheck2)
                {
                    CGS_ASSERT(lpaReceivedClientsIDs[liCheck2] == KI_INVALID_PLAYER_ID ||
                                   lpaReceivedClientsIDs[liCheck1] != lpaReceivedClientsIDs[liCheck2],
                               "laBestReceivedClientsIDs[lnCheck2] == K_INVALID_PLAYER_ID || "
                               "laBestReceivedClientsIDs[lnCheck1] != laBestReceivedClientsIDs[lnCheck2]");
                }
            }

            Message* lpMessage = lpNetPlayer->GetRegisteredSendMessage(KI_MESSAGE_TYPE_NEW_HOST);
            static_cast<NewHostMessage*>(lpMessage)->PrepareForSend(lu16CurrentFrame, liNewHostID,
                                                                    lpaReceivedClientsIDs);
        }
    }

    // -------------------------------------------------------------------------------
    // The current host is silent: pick the next host in the election list (or ourselves if we
    // are last), broadcast a NewHostMessage naming it, and adopt it as the new host.
    void HostMigrationManager::FindNextHostAndSend(const CgsSystem::TimerStatus* lpTimerStatus,
                                                   u16 lu16CurrentFrame)
    {
        NetworkPlayerID liLocalPlayerID = KI_INVALID_PLAYER_ID;
        mpPlayerManager->GetNextLocalPlayerID(&liLocalPlayerID);

        // Locate the current host in the election list.
        s32 liHostIndex = 0;
        while (liHostIndex < KI_MAX_PLAYERS && maHostMigrationPlayerIDs[liHostIndex] != mHostPlayerID)
            ++liHostIndex;

        // Scan forward from the host for the first still-connected candidate.
        NetworkPlayerID liNextHostID = mHostPlayerID;
        s32 liIndex = liHostIndex + 1;
        bool lbFoundCandidate = false;
        for (; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            const NetworkPlayerID liCandidate = maHostMigrationPlayerIDs[liIndex];
            if (liCandidate == liLocalPlayerID)
            {
                *CgsDev::Log::gpDebugPrint
                    << "We are the next player in the list of host migration IDs\n";
                lbFoundCandidate = true;
                liNextHostID = liCandidate;
                break;
            }

            NetworkPlayer* lpCandidate = mpPlayerManager->GetPlayerByID(liCandidate);
            if (lpCandidate != nullptr)
            {
                *CgsDev::Log::gpDebugPrint << "Network player " << static_cast<s32>(liCandidate)
                                           << " is the next player in the list of host migration IDs\n";
                lbFoundCandidate = true;
                liNextHostID = liCandidate;
                break;
            }
        }

        if (!lbFoundCandidate)
        {
            // No connected successor: migrate the host to ourselves.
            *CgsDev::Log::gpDebugPrint << "Current host is the last choice - migrating host to me ("
                                       << static_cast<s32>(liLocalPlayerID) << ")\n";
            liNextHostID = liLocalPlayerID;
        }
        else
        {
            CGS_ASSERT(liIndex >= 0, "i >= 0");
        }

        // Broadcast the new-host decision to every connected player. The received-clients list
        // starts with just our local id; everyone else is pending acknowledgement.
        NetworkPlayerID laReceivedClientsIDs[KI_MAX_PLAYERS];
        laReceivedClientsIDs[0] = liLocalPlayerID;
        for (s32 liInit = 1; liInit < KI_MAX_PLAYERS; ++liInit)
            laReceivedClientsIDs[liInit] = KI_INVALID_PLAYER_ID;

        NetworkPlayerID liPlayerID = KI_INVALID_PLAYER_ID;
        for (bool lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
             lbHasPlayer;
             lbHasPlayer = mpPlayerManager->GetNextPlayerID(
                 &liPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
            if (lpNetPlayer == nullptr)
                continue;

            for (s32 liCheck1 = 1; liCheck1 < KI_MAX_PLAYERS; ++liCheck1)
            {
                for (s32 liCheck2 = liCheck1 + 1; liCheck2 < KI_MAX_PLAYERS; ++liCheck2)
                {
                    CGS_ASSERT(laReceivedClientsIDs[liCheck2] == KI_INVALID_PLAYER_ID ||
                                   laReceivedClientsIDs[liCheck1] != laReceivedClientsIDs[liCheck2],
                               "laReceivedClientsIDs[lnCheck2] == K_INVALID_PLAYER_ID || "
                               "laReceivedClientsIDs[lnCheck1] != laReceivedClientsIDs[lnCheck2]");
                }
            }

            Message* lpMessage = lpNetPlayer->GetRegisteredSendMessage(KI_MESSAGE_TYPE_NEW_HOST);
            static_cast<NewHostMessage*>(lpMessage)->PrepareForSend(lu16CurrentFrame, liNextHostID,
                                                                    laReceivedClientsIDs);
        }

        SetNewHost(liNextHostID, lpTimerStatus, lu16CurrentFrame);
    }

    // -------------------------------------------------------------------------------
    bool HostMigrationManager::Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame)
    {
        bool lbResult = false;

        if (mHostPlayerID == KI_INVALID_PLAYER_ID)
            return lbResult;

        // Consume any host keep-alive we received this frame.
        NetworkPlayer* lpHostPlayer = mpPlayerManager->GetPlayerByID(mHostPlayerID);
        if (lpHostPlayer != nullptr)
        {
            HostKeepAliveMessage* lpKeepAlive = static_cast<HostKeepAliveMessage*>(
                lpHostPlayer->GetRegisteredRecvMessage(KI_MESSAGE_TYPE_HOST_KEEP_ALIVE));
            if (lpKeepAlive->Retrieve())
                mu16LastHostKeepAliveReceivedTime = lu16CurrentFrame;
        }

        // Elect the best host from all received messages.
        NetworkPlayerID liBestKeepAliveHostID = KI_INVALID_PLAYER_ID;
        NetworkPlayerID liBestNewHostID       = KI_INVALID_PLAYER_ID;
        NetworkPlayerID laBestReceivedClientsIDs[KI_MAX_PLAYERS];
        FindBestHostIDs(&liBestKeepAliveHostID, &liBestNewHostID, laBestReceivedClientsIDs);

        CGS_ASSERT(liBestKeepAliveHostID != KI_INVALID_PLAYER_ID,
                   "lBestKeepAliveHostID != K_INVALID_PLAYER_ID");
        CGS_ASSERT(liBestNewHostID != KI_INVALID_PLAYER_ID,
                   "lBestNewHostID != K_INVALID_PLAYER_ID");

        SendNewHostMessageIfRequired(lu16CurrentFrame, liBestNewHostID, laBestReceivedClientsIDs);

        // The keep-alive winner overrides the new-host winner if it ranks earlier.
        NetworkPlayerID liChosenHostID = liBestNewHostID;
        if (ABecomesHostBeforeB(liBestNewHostID, liBestKeepAliveHostID))
            liChosenHostID = liBestKeepAliveHostID;

        if (ABecomesHostBeforeB(mHostPlayerID, liChosenHostID))
        {
            lbResult = true;
            SetNewHost(liChosenHostID, lpTimerStatus, lu16CurrentFrame);
            *CgsDev::Log::gpDebugPrint << "[HOST MIGRATION] Set host to "
                                       << static_cast<s32>(liChosenHostID) << "\n";
        }

        if (lpHostPlayer != nullptr)
        {
            if (!lbResult)
            {
                if (!IsHostAlive(lu16CurrentFrame))
                    FindNextHostAndSend(lpTimerStatus, lu16CurrentFrame);
            }
        }
        else
        {
            // We are the host: keep broadcasting our heartbeat.
            UpdateSendingOfHostKeepAliveMessage(lu16CurrentFrame);
        }

        return lbResult;
    }
} // namespace CgsNetwork
