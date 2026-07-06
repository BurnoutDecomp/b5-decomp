#include "GameShared/GameClasses/Network/Debug Components/CgsNetworkHostMigrationDebugComponent.h"
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::HostMigrationDebugComponent::ForceHostMigration  @ 0x8287F398
//   CgsNetwork::HostMigrationDebugComponent::OnActivate          @ 0x8288BCD8
//   CgsNetwork::HostMigrationDebugComponent::UpdateLocalPlayerID @ 0x8287F408
//
// The migration-debug component's action + activation bodies. It reaches into the owning
// HostMigrationManager's private keep-alive / host-id / player-manager state (befriended in
// CgsHostMigrationManager.h). See the header for the layout notes.

namespace CgsNetwork
{
    // @ 0x8287F398. "Force host migration" menu action: rewind the last-keep-alive-received
    // time by twice the keep-alive timeout so the host-alive watchdog (IsHostAlive) trips and
    // a migration election is forced. The u16 field wraps -- this is a deliberate underflow.
    void HostMigrationDebugComponent::ForceHostMigration(void* lpData)
    {
        HostMigrationDebugComponent* lpHostMigrationDebugComponent =
            static_cast<HostMigrationDebugComponent*>(lpData);
        CGS_ASSERT(lpHostMigrationDebugComponent != nullptr, "lpHostMigrationDebugComponent");

        HostMigrationManager* lpHostMigrationManager =
            lpHostMigrationDebugComponent->mpHostMigrationManager;
        lpHostMigrationManager->mu16LastHostKeepAliveReceivedTime +=
            static_cast<u16>(-2 * lpHostMigrationManager->mu16HostKeepAliveTimeout);
    }

    // @ 0x8288BCD8. Register the debug-menu surface when the component is activated: the two
    // read-only ID watches (local player, host player) and the two migration actions.
    void HostMigrationDebugComponent::OnActivate()
    {
        RegisterVariable(&mLocalPlayerID, "Local Player ID");
        SetReadOnly(&mLocalPlayerID, true);

        RegisterVariable(&mpHostMigrationManager->mHostPlayerID, "Host Player ID");
        SetReadOnly(&mpHostMigrationManager->mHostPlayerID, true);

        RegisterFunction(&HostMigrationDebugComponent::ForceHostMigration, this, "Force host migration");
        RegisterFunction(&HostMigrationDebugComponent::UpdateLocalPlayerID, this, "Update Local Player ID");
    }

    // @ 0x8287F408. "Update Local Player ID" menu action: reset the displayed local-player-ID
    // watch, then re-read it from the PlayerManager. Because the watch is cleared to -1 first,
    // the (mLocalPlayerID != -1) guard is always false at the read; the net effect is
    // mLocalPlayerID <- PlayerManager local player ID (or -1 if that ID is itself -1). The
    // redundant guard is reproduced as emitted by the X360.
    void HostMigrationDebugComponent::UpdateLocalPlayerID(void* lpData)
    {
        HostMigrationDebugComponent* lpDebugComponent =
            static_cast<HostMigrationDebugComponent*>(lpData);
        CGS_ASSERT(lpDebugComponent != nullptr, "lpDebugComponent");

        lpDebugComponent->mLocalPlayerID = -1;
        CGS_ASSERT(lpDebugComponent->mpHostMigrationManager != nullptr,
                   "lpDebugComponent->mpHostMigrationManager");
        CGS_ASSERT(lpDebugComponent->mpHostMigrationManager->mpPlayerManager != nullptr,
                   "lpDebugComponent->mpHostMigrationManager->mpPlayerManager");

        const NetworkPlayerID liLocalPlayerID =
            lpDebugComponent->mpHostMigrationManager->mpPlayerManager->GetLocalPlayerID();
        if (lpDebugComponent->mLocalPlayerID != -1 || liLocalPlayerID == -1)
        {
            lpDebugComponent->mLocalPlayerID = -1;
        }
        else
        {
            lpDebugComponent->mLocalPlayerID = liLocalPlayerID;
        }
    }
}
