#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFont.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // DrawLine
#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFontData.h"                   // the carved glyph data

// CgsDev::VectorFont bodies (X360 Print 0x8281FBF8 / PrintComplex 0x8281F8A8). A glyph is drawn as its
// CharLine strokes: each stroke endpoint is (pen + coord * mSize); the pen advances by KAN_CHARWIDTH.
// The stroke coords live in an 8x8 cell (KF_CHARWIDTH/HEIGHT), so SetSize divides the requested text
// size by the cell to get the per-coord scale.

namespace CgsDev
{
    using namespace CompressedFontData;

    void VectorFont::Construct()
    {
        mSize = { 1.0f, 1.0f, 0.0f, 0.0f };
        mColour = 0xFFFFFFFFu;
        mp2DRenderer = nullptr;
    }

    void VectorFont::SetSize(Vector2 lSize)
    {
        mSize.x = lSize.x / KF_CHARWIDTH;
        mSize.y = lSize.y / KF_CHARHEIGHT;
        mSize.z = 0.0f;
        mSize.w = 0.0f;
    }

    void VectorFont::DrawLine(Vector2 lP1, Vector2 lP2, u32 luColour)
    {
        if (mp2DRenderer)
            mp2DRenderer->DrawLine(lP1, lP2, luColour);
    }

    f32 VectorFont::Print(f32 lfX, f32 lfY, const char* lpcString, u32 luColour)
    {
        if (!lpcString)
            return lfX;

        mColour = luColour;
        f32 lfPenX = lfX;
        const f32 lfPenY = lfY;

        for (const unsigned char* lpc = reinterpret_cast<const unsigned char*>(lpcString); *lpc; ++lpc)
        {
            const s32 liChar = static_cast<s32>(*lpc);
            if (liChar < KI_FIRST_CHAR || liChar > KI_LAST_CHAR)
                continue;   // outside the charset (incl. control chars) - skip

            const s32        liIndex = liChar - KI_FIRST_CHAR;
            const s32        liLines = static_cast<s32>(KAN_LINECOUNT[liIndex]);
            const CharLine*  lpLines = KA_CHARSET[liIndex];

            for (s32 liLine = 0; liLine < liLines; ++liLine)
            {
                const CharLine& lrLine = lpLines[liLine];
                const Vector2 lP1 = { lfPenX + static_cast<f32>(lrLine.miStartX) * mSize.x,
                                      lfPenY + static_cast<f32>(lrLine.miStartY) * mSize.y, 0.0f, 0.0f };
                const Vector2 lP2 = { lfPenX + static_cast<f32>(lrLine.miEndX)   * mSize.x,
                                      lfPenY + static_cast<f32>(lrLine.miEndY)   * mSize.y, 0.0f, 0.0f };
                DrawLine(lP1, lP2, luColour);
            }

            lfPenX += static_cast<f32>(KAN_CHARWIDTH[liIndex]) * mSize.x;
        }

        return lfPenX;
    }
}
