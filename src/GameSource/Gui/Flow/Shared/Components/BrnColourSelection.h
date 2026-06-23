#pragma once

// ===================================================================================
// BrnGui::ColourSelection  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h
//
// The shared colour-picker GUI component. GetUint32ColourFromVector4 (@0x824E8528) packs
// a normalised RGBA colour vector (0..1 floats) into a 0x00RRGGBB integer (the alpha
// channel is dropped). Only this one helper is binary-recovered for this TU; the picker's
// member layout is not referenced by it and is not modelled here.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    class ColourSelection
    {
    public:
        // @ 0x824E8528 - pack a normalised colour vector (lpv4Colour[0..2] = R,G,B in 0..1)
        // into 0x00RRGGBB. Each channel is scaled by 255.0 and truncated toward zero; the
        // alpha lane (lpv4Colour[3]) is not used.
        static s32 GetUint32ColourFromVector4(const f32* lpv4Colour);
    };
}
