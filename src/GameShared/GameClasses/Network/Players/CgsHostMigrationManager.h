#pragma once

// ===================================================================================
// CgsNetwork::HostMigrationManager / HostMigrationNetworkPlayerData -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h
//
// The peer-to-peer host-migration subsystem. Each session peer runs one of these. It
//   * broadcasts a periodic HostKeepAliveMessage so clients know the host lives,
//   * watches for the host going silent (IsHostAlive),
//   * runs a deterministic "who becomes host next" election (ABecomesHostBeforeB /
//     QSortCallback over maHostMigrationPlayerIDs) and broadcasts NewHostMessages,
//   * and, on a decision, re-points the session host (SetNewHost -> PlayerManager +
//     the registered HostMigrationCallbacks).
//
// SHAPE is authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsHostMigrationManager.h), gated against the
// X360 ARTIST binary. Every member offset below is the byte offset the ARTIST asm
// dereferences (Construct/Prepare/Release init each field at these offsets):
//
//   +0x000  maMessageData[7]               (HostMigrationNetworkPlayerData, 204B each
//                                           -> Prepare strides 51 dwords == 204 bytes;
//                                              mOwnerPlayer at the record's +0)
//   +0x594  mHostPlayerID                  (NetworkPlayerID s32)        ( = 1428)
//   +0x598  maHostMigrationPlayerIDs[8]    (NetworkPlayerID[8])         ( = 1432)
//   +0x5B8  mu16LastHostKeepAliveSendTime  (u16)                        ( = 1464)
//   +0x5BA  mu16LastHostKeepAliveReceivedTime (u16)                     ( = 1466)
//   +0x5BC  mapHostMigrationCallback[2]    (HostMigrationCallback*[2])  ( = 1468)
//   +0x5C4  mapHostMigrationCallbackData[2](void*[2])                   ( = 1476)
//   +0x5CC  mpPlayerManager                (PlayerManager*)             ( = 1484)
//   +0x5D0  mu16HostKeepAliveSendTimeout   (u16)                        ( = 1488)
//   +0x5D2  mu16HostKeepAliveTimeout       (u16)                        ( = 1490)
//   +0x5D4  mu16HostKeepAliveInitialTimeout(u16)                        ( = 1492)
//   +0x5D8  meFrameRate                    (CgsSystem::EFrameRate s32)  ( = 1496)
//   +0x5DC  mHostMigrationDebugComponent   (HostMigrationDebugComponent)( = 1500)
//
// WIDTH MODELLING: the offsets above are the X360 (32-bit-pointer) byte offsets, kept
// as documentation. The reconstruction accesses every field BY NAME (never by raw
// offset), so the C++ struct uses native x64 widths and the compiler lays it out
// naturally -- semantic parity, not byte-exact X360 layout (the rule the rest of the
// project follows; cf. CgsNetworkPlayer.h / CgsMessage.h).
//
// IsHostAlive @ 0x82872258 was reconstructed in another TU (CgsMessageSubclasses.cpp)
// against a minimal placeholder of this class in CgsMessage.h. That placeholder is the
// catch-all-TU stand-in; this header is the canonical home. IsHostAlive is declared
// here so Update can call it by name; its body lives in CgsMessageSubclasses.cpp.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"        // EFrameRate, NetworkPlayer, NetworkPlayerID
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHostKeepAliveMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsNewHostMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"  // NetworkPlayerID typedef
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Network/Debug Components/CgsNetworkHostMigrationDebugComponent.h"  // HostMigrationDebugComponent (canonical home)

namespace CgsSystem
{
    class TimerStatus;   // fwd: passed through to the host-migration callbacks only.
}

namespace CgsNetwork
{
    struct PlayerManager;

    // ------------------------------------------------------------------------------
    // One per-peer slot of host-migration message state (DWARF CgsHostMigrationManager.h:55).
    // sizeof == 204 bytes (the Prepare init loop strides 51 dwords between mOwnerPlayer fields).
    struct HostMigrationNetworkPlayerData
    {
        // The shared signed player-index typedef (-1 == "no player").
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        NetworkPlayerID    mOwnerPlayer;             // +0x00  (Prepare/Release init: -1)
        HostKeepAliveMessage mHostKeepAliveMessageSend; // +0x04
        HostKeepAliveMessage mHostKeepAliveMessageRecv; // +0x24
        NewHostMessage       mNewHostMessageSend;       // +0x44
        NewHostMessage       mNewHostMessageRecv;       // +0x84
    };

    // ------------------------------------------------------------------------------
    // The manager's debug-menu component (DWARF CgsHostMigrationManager.h:222) lives in its own
    // home now (Network/Debug Components/CgsNetworkHostMigrationDebugComponent.h, included above).
    // Construct stores a back-pointer to the owning manager and a -1 sentinel, then Register()s it.

    // ------------------------------------------------------------------------------
    struct HostMigrationManager
    {
        // The debug component reads this manager's private keep-alive / host-id / player-manager
        // members directly (ForceHostMigration / OnActivate / UpdateLocalPlayerID).
        friend struct HostMigrationDebugComponent;

        // The shared signed player-index typedef (-1 == "no player").
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        // Host-migration callback: fires whenever the session host changes.
        typedef void (*HostMigrationCallback)(const CgsSystem::TimerStatus* lpTimerStatus,
                                              NetworkPlayerID liOldHostID,
                                              NetworkPlayerID liNewHostID,
                                              void* lpUserData);

        static const s32 KI_MAX_NETWORK_PLAYERS        = 7;  // maMessageData size
        static const s32 KI_MAX_PLAYERS                = 8;  // maHostMigrationPlayerIDs size
        static const s32 KI_MAX_HOST_MIGRATION_CALLBACKS = 2;

        // --- public lifecycle / API ---
        void            Construct();
        bool            Prepare(PlayerManager* lpPlayerManager,
                                CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                u16 lu16CurrentFrame);
        bool            Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);
        bool            Release();
        void            Destruct();

        NetworkPlayerID GetHostPlayerID();

        void            Enable(NetworkPlayerID liGameRoomHostID,
                               const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);
        void            Disable(NetworkPlayerID liGameRoomHostID,
                                const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);

        void            AddPlayer(NetworkPlayerID liPlayerID);
        void            RemovePlayer(NetworkPlayerID liPlayerID);
        void            Disconnected();

        void            RegisterHostMigrationCallback(HostMigrationCallback lpfCallback, void* lpUserData);
        void            UnregisterHostMigrationCallback(HostMigrationCallback lpfCallback);

        bool            ABecomesHostBeforeB(NetworkPlayerID liPlayerIDa, NetworkPlayerID liPlayerIDb);

        // qsort comparator over maHostMigrationPlayerIDs (uses gQuickSortHostPlayerID).
        static s32      QSortCallback(const void* lpData1, const void* lpData2);

        // IsHostAlive @ 0x82872258 -- bodied in CgsMessageSubclasses.cpp (declared here so
        // Update can call it by name). Reads mu16LastHostKeepAliveReceivedTime / mu16HostKeepAliveTimeout.
        bool            IsHostAlive(u16 lu16CurrentFrame) const;

    private:
        void            SetFrameRate(CgsSystem::EFrameRate leFrameRate);
        void            RegisterMessages(NetworkPlayerID liPlayerID);
        void            UnregisterMessages(NetworkPlayerID liPlayerID);
        void            UpdateSendingOfHostKeepAliveMessage(u16 lu16CurrentFrame);
        void            SetNewHost(NetworkPlayerID liNewHostID,
                                   const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);
        void            SendHostKeepAliveMessage(NetworkPlayer* lpNetPlayer, u16 lu16CurrentFrame);
        void            FindBestHostIDs(NetworkPlayerID* lpBestHostKeepAlivePlayerID,
                                        NetworkPlayerID* lpBestNewHostPlayerID,
                                        NetworkPlayerID* lpaBestReceivedClientsIDs);
        void            SendNewHostMessageIfRequired(u16 lu16CurrentFrame, NetworkPlayerID liNewHostID,
                                                     NetworkPlayerID* lpaReceivedClientsIDs);
        void            FindNextHostAndSend(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);
        void            ClearAllCallbacks();

        // --- layout (frozen against the ARTIST asm offsets; accessed BY NAME) ---
        HostMigrationNetworkPlayerData maMessageData[KI_MAX_NETWORK_PLAYERS]; // +0x000
        NetworkPlayerID  mHostPlayerID;                                       // +0x594
        NetworkPlayerID  maHostMigrationPlayerIDs[KI_MAX_PLAYERS];            // +0x598
        u16              mu16LastHostKeepAliveSendTime;                       // +0x5B8
        u16              mu16LastHostKeepAliveReceivedTime;                   // +0x5BA
        HostMigrationCallback mapHostMigrationCallback[KI_MAX_HOST_MIGRATION_CALLBACKS];     // +0x5BC
        void*            mapHostMigrationCallbackData[KI_MAX_HOST_MIGRATION_CALLBACKS];      // +0x5C4
        PlayerManager*   mpPlayerManager;                                     // +0x5CC
        u16              mu16HostKeepAliveSendTimeout;                        // +0x5D0
        u16              mu16HostKeepAliveTimeout;                            // +0x5D2
        u16              mu16HostKeepAliveInitialTimeout;                     // +0x5D4
        CgsSystem::EFrameRate meFrameRate;                                    // +0x5D8
        HostMigrationDebugComponent mHostMigrationDebugComponent;             // +0x5DC
    };
} // namespace CgsNetwork
