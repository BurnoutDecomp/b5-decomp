#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFont.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // DrawLine
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
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
        mSize = Vector2(1.0f, 1.0f);
        mColour = 0xFFFFFFFFu;
        mp2DRenderer = 0;
        mp3DRenderer = 0;
    }

    void VectorFont::SetSize(Vector2 lSize)
    {
        // ARTIST's inlined store (CgsAssert::Construct 0x828207E4..0x828207F4)
        // splats 0.125 and uses one VMX multiply across the full register.
        const __vector4 lvScale = { 0.125f, 0.125f, 0.125f, 0.125f };
        mSize.mV = __vmulfp(lSize.mV, lvScale);
    }

    void VectorFont::DrawLine(Vector2 lP1, Vector2 lP2, u32 luColour)
    {
        if (mp2DRenderer)
            mp2DRenderer->DrawLine(lP1, lP2, luColour);
        else
        {
            // ARTIST 8281F18C..8281F244 widens the screen-space endpoints
            // with zero Z/W and keeps the strokes in the owning 3D debug bank.
            CGS_ASSERT(mp3DRenderer != 0, "mp3DRenderer");
            rw::RGBA lColour;
            lColour.m_rgba = luColour;
            mp3DRenderer->Draw2DLineForVectorFont(
                rw::math::vpu::Vector3(lP1.mV.x, lP1.mV.y, 0.0f),
                rw::math::vpu::Vector3(lP2.mV.x, lP2.mV.y, 0.0f), lColour);
        }
    }

    // X360 0x82818E10. Character advances use the VMX-scaled font size; the native
    // tab branch multiplies the width at KAN_CHARWIDTH[32] by four.
    Vector2 VectorFont::ComputeTextExtent(const char* lpcText, Vector2 lTextSize)
    {
        if (!lpcText)
            return Vector2(0.0f, 0.0f);

        const __vector4 lvScale = { 0.125f, 0.125f, 0.125f, 0.125f };
        const __vector4 lvFontSize = __vmulfp(lTextSize.mV, lvScale);
        f32 lfLongestLine = 0.0f;
        f32 lfLineLength = 0.0f;
        f32 lfHeight = 0.0f;

        // ARTIST uses the width at table index 32 as its tab cell, then applies
        // the tab's factor of four in a second VMX multiply.
        const __vector4 lvTabWidth = { static_cast<f32>(KAN_CHARWIDTH[32]),
                                       static_cast<f32>(KAN_CHARWIDTH[32]),
                                       static_cast<f32>(KAN_CHARWIDTH[32]),
                                       static_cast<f32>(KAN_CHARWIDTH[32]) };
        const __vector4 lvFour = { 4.0f, 4.0f, 4.0f, 4.0f };
        const __vector4 lvEight = { KF_CHARHEIGHT, KF_CHARHEIGHT,
                                    KF_CHARHEIGHT, KF_CHARHEIGHT };

        for (const unsigned char* lpcString = reinterpret_cast<const unsigned char*>(lpcText);
             *lpcString; ++lpcString)
        {
            const unsigned char lcCharacter = *lpcString;
            if (lcCharacter == '\n')
            {
                if (lfLineLength > lfLongestLine)
                    lfLongestLine = lfLineLength;
                lfLineLength = 0.0f;
                lfHeight += __vmulfp(__vspltw(lvFontSize, 1), lvEight).x;
            }
            else if (lcCharacter == '\t')
            {
                __vector4 lvTabAdvance = __vmulfp(__vspltw(lvFontSize, 0), lvTabWidth);
                lvTabAdvance = __vmulfp(lvTabAdvance, lvFour);
                lfLineLength += lvTabAdvance.x;
            }
            else if (lcCharacter <= KI_LAST_CHAR)
            {
                const f32 lfCharacterWidth = static_cast<f32>(
                    KAN_CHARWIDTH[lcCharacter - KI_FIRST_CHAR]);
                const __vector4 lvWidth = { lfCharacterWidth, lfCharacterWidth,
                                            lfCharacterWidth, lfCharacterWidth };
                const __vector4 lvAdvance = __vmulfp(__vspltw(lvFontSize, 0), lvWidth);
                lfLineLength += lvAdvance.x;
            }
        }

        if (lfLineLength > lfLongestLine)
            lfLongestLine = lfLineLength;
        if (lfLineLength > 0.0f)
            lfHeight += __vmulfp(__vspltw(lvFontSize, 1), lvEight).x;

        return Vector2(lfLongestLine, lfHeight);
    }

    f32 VectorFont::Print(f32 lfX, f32 lfY, const char* lpcString, u32 luColour)
    {
        if (!lpcString)
            return lfY + mSize.mV.y * KF_CHARHEIGHT;

        mColour = luColour;
        f32 lfPenX = lfX;
        f32 lfPenY = lfY;

        for (const unsigned char* lpc = reinterpret_cast<const unsigned char*>(lpcString); *lpc; ++lpc)
        {
            const s32 liChar = static_cast<s32>(*lpc);
            if (liChar == '\n')
            {
                lfPenX = lfX;
                lfPenY += mSize.mV.y * KF_CHARHEIGHT;
                continue;
            }
            if (liChar == '\t')
            {
                const f32 lfTabCharWidth = static_cast<f32>(KAN_CHARWIDTH[32]);
                const __vector4 lvTabCharWidth = { lfTabCharWidth, lfTabCharWidth,
                                                   lfTabCharWidth, lfTabCharWidth };
                const __vector4 lvFour = { 4.0f, 4.0f, 4.0f, 4.0f };
                __vector4 lvTabAdvance = __vmulfp(__vspltw(mSize.mV, 0), lvTabCharWidth);
                lvTabAdvance = __vmulfp(lvTabAdvance, lvFour);
                lfPenX += lvTabAdvance.x;
                continue;
            }
            if (liChar < 0x1F || liChar > KI_LAST_CHAR)
                continue;

            const s32        liIndex = liChar - KI_FIRST_CHAR;
            const s32        liLines = static_cast<s32>(KAN_LINECOUNT[liIndex]);
            const CharLine*  lpLines = KA_CHARSET[liIndex];

            for (s32 liLine = 0; liLine < liLines; ++liLine)
            {
                const CharLine& lrLine = lpLines[liLine];
                const Vector2 lP1(lfPenX + static_cast<f32>(lrLine.miStartX) * mSize.mV.x,
                                  lfPenY + static_cast<f32>(lrLine.miStartY) * mSize.mV.y);
                const Vector2 lP2(lfPenX + static_cast<f32>(lrLine.miEndX) * mSize.mV.x,
                                  lfPenY + static_cast<f32>(lrLine.miEndY) * mSize.mV.y);
                DrawLine(lP1, lP2, luColour);
            }

            lfPenX += static_cast<f32>(KAN_CHARWIDTH[liIndex]) * mSize.mV.x;
        }

        return lfPenY + mSize.mV.y * KF_CHARHEIGHT;
    }
}
