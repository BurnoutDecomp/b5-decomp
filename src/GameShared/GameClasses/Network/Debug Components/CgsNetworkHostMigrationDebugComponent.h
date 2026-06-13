#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::HostMigrationDebugComponent::GetName @ 0x827DBBF0  -> "Host Migration Manager"
//   CgsNetwork::HostMigrationDebugComponent::GetPath @ 0x827DBBE0  -> "Network"
//
// Debug-UI identity hooks for the host-migration debug component. Header-keyed TU;
// members inline.

namespace CgsNetwork
{
    class HostMigrationDebugComponent
    {
    public:
        const char* GetName() const { return "Host Migration Manager"; }
        const char* GetPath() const { return "Network"; }
    };
}
