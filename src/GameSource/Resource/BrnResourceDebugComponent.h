#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnResource::DebugComponent)
//
//   GetName @ 0x827DD200:
//       return "GameDataModule";
//
// Debug component identity hook for the resource module; returns the constant
// label shown in the debug UI. Header-keyed TU: the single recovered member is
// defined inline.

namespace BrnResource
{
    class DebugComponent
    {
    public:
        const char* GetName() { return "GameDataModule"; }
    };
}
