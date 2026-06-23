#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnResource::DebugComponent)
//
//   GetName @ 0x827DD200:
//       return "GameDataModule";
//
// Debug component identity hook for the resource module; returns the constant
// label shown in the debug UI. GetName is defined inline; InterpolateColours is
// defined in BrnResourceDebugComponent.cpp.

namespace BrnResource
{
    class DebugComponent
    {
    public:
        const char* GetName() { return "GameDataModule"; }

        // Per-channel linear interpolation of two packed 0xAARRGGBB colours, used by
        // RenderHUD to fade the resource-bar colours. lfFactor selects toward luColourT
        // (factor 1 == luColourT, factor 0 == luColour0). Each channel is rounded to
        // nearest (add 0.5, truncate). X360 0x82661410.
        static uint32_t InterpolateColours(uint32_t luColourT, uint32_t luColour0, f32 lfFactor);
    };
}
