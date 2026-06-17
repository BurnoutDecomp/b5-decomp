#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2)

// CgsDev::VectorFont - the debug VECTOR font. Each glyph is a set of line strokes (CompressedFontData,
// carved from the X360 into CgsVectorFontData.h); Print walks a string and draws each glyph's strokes
// via the 2D debug renderer's DrawLine. Recovered from the Feb-2007 leak interface + the X360
// (Print 0x8281FBF8 / PrintComplex 0x8281F8A8). This is the real debug font (no texture/atlas needed -
// the glyph stroke data is source-baked). The 3D renderer path + VectorFontStream are the follow-on.

namespace CgsDev
{
    struct Debug2DImmediateRender;

    class VectorFont
    {
    public:
        void Construct();
        void SetSize(Vector2 lSize);                       // text size in px; cell-normalised internally
        void SetColour(u32 luColour) { mColour = luColour; }
        void SetRenderer(Debug2DImmediateRender* lpRenderer) { mp2DRenderer = lpRenderer; }

        // Draw lpcString at (lfX, lfY) in luColour; returns the pen X after the string.
        f32 Print(f32 lfX, f32 lfY, const char* lpcString, u32 luColour);

    private:
        void DrawLine(Vector2 lP1, Vector2 lP2, u32 luColour);

        Vector2                 mSize;          // per-coord scale (lSize / KF_CHAR*)
        u32                     mColour;
        Debug2DImmediateRender* mp2DRenderer;
    };
}
