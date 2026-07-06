#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent base
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                // CgsNetwork::NetworkPlayerID

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::HostMigrationDebugComponent::GetName             @ 0x827DBBF0  -> "Host Migration Manager"
//   CgsNetwork::HostMigrationDebugComponent::GetPath             @ 0x827DBBE0  -> "Network"
//   CgsNetwork::HostMigrationDebugComponent::ForceHostMigration  @ 0x8287F398
//   CgsNetwork::HostMigrationDebugComponent::OnActivate          @ 0x8288BCD8
//   CgsNetwork::HostMigrationDebugComponent::UpdateLocalPlayerID @ 0x8287F408
//
// The host-migration manager's debug-menu component: it exposes the local/host player IDs as
// read-only watches and two migration actions ("Force host migration", "Update Local Player ID").
// A CgsDev::DebugComponent subclass. HostMigrationManager embeds one of these by value and
// Construct()s it (mpHostMigrationManager = &owner, mLocalPlayerID = -1, then Register()).
//
// LAYOUT (X360 offsets, accessed BY NAME): the DebugComponent base occupies +0x00..+0x0C, then
// this class adds mpHostMigrationManager @+0x0C and mLocalPlayerID @+0x10.

namespace CgsNetwork
{
    struct HostMigrationManager;   // fwd: the component holds a back-pointer only

    struct HostMigrationDebugComponent : public CgsDev::DebugComponent
    {
    public:
        HostMigrationManager* mpHostMigrationManager;  // +0x0C (Construct: = &owner)
        NetworkPlayerID       mLocalPlayerID;          // +0x10 (Construct: = -1)

        // X360 0x8287F398. "Force host migration" menu action (static DebugCallbackFunction).
        static void ForceHostMigration(void* lpData);
        // X360 0x8287F408. "Update Local Player ID" menu action (static DebugCallbackFunction).
        static void UpdateLocalPlayerID(void* lpData);

    protected:
        // X360 0x827DBBF0 / 0x827DBBE0. Debug-menu identity.
        virtual const char* GetName() const { return "Host Migration Manager"; }
        virtual const char* GetPath() const { return "Network"; }
        // X360 0x8288BCD8. Register the read-only watches + the migration actions.
        virtual void        OnActivate();
    };
}
