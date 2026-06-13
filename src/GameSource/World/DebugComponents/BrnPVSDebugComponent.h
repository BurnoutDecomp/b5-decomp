#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::PVSDebugComponent::GetName  @ 0x827DD240  -> "PVS"
//   BrnWorld::PVSDebugComponent::IsSimple @ 0x827E2F38  -> false
//
// Debug-UI identity hooks for the PVS (potentially-visible-set) debug component.
// Header-keyed TU; members inline.

namespace BrnWorld
{
    class PVSDebugComponent
    {
    public:
        const char* GetName() const { return "PVS"; }
        bool        IsSimple() const { return false; }
    };
}
