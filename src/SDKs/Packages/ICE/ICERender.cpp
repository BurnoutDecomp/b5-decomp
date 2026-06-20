// ============================================================================
// SDKs/Packages/ICE/ICERender.cpp
//
// ICE::ICERender -- the ICE camera-take debug-overlay draw surface. Four static
// helpers that marshal box/text draws into the CgsDev debug-render subsystem:
//   * RenderPoly       @0x82533578  -- a 2D debug box around a projected poly
//   * ScrPrintfArg     @0x82533750  -- the core: vsnprintf -> Draw2DText
//   * ScrPrintf        @0x825343D0  -- white-text vararg front end -> ScrPrintfArg
//   * ScrShadowPrintf  @0x825342E0  -- ScrPrintfArg x3 (dark shadow pass + main)
//
// Each draw acquires a stack DebugInterface, queues the 2D prim through its
// Get2dRender() buffered renderer, then thread-safe-releases the manager.
//
// COLOUR: the colour params arrive as ICE::Vector4 with one float per channel in
// [0,255]; the X360 clamps each lane to [0,255] and packs it to a CgsDev::RGBA
// (packed RGBA8 u32) before the Draw2D* call. PackColourRGBA reproduces that.
// ============================================================================

#include "SDKs/Packages/ICE/ICERender.hpp"
#include "rw/core/stdc/stdc.h"   // rw::core::stdc::Vsnprintf (the text formatter)

#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface (+ DebugManager)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"         // CgsDev::DebugRender / RGBA / Vector2

namespace ICE
{
    // ICE's menu-anchor shift (the editor overlay origin), from the X360 inline
    // constants (KI_ICE_MENU_SHIFT_X / _Y). All ICE debug draws are offset by this.
    static const s32 KI_ICE_MENU_SHIFT_X = 10;
    static const s32 KI_ICE_MENU_SHIFT_Y = 120;

    namespace
    {
        // Clamp `lfChannel` to [0,255] (the X360 dual-fsel: max(0) then min(255)).
        inline f32 ClampColourChannel(f32 lfChannel)
        {
            if (lfChannel < 0.0f)   lfChannel = 0.0f;
            if (lfChannel > 255.0f) lfChannel = 255.0f;
            return lfChannel;
        }

        // Pack a per-channel-[0,255] colour vector to a CgsDev::RGBA (packed RGBA8),
        // R in the high byte through A in the low byte -- the X360 fctidz/insrwi pack.
        inline CgsDev::RGBA PackColourRGBA(const Vector4& lrColour)
        {
            const u32 luR = (u32)(s32)ClampColourChannel(lrColour.x);
            const u32 luG = (u32)(s32)ClampColourChannel(lrColour.y);
            const u32 luB = (u32)(s32)ClampColourChannel(lrColour.z);
            const u32 luA = (u32)(s32)ClampColourChannel(lrColour.w);
            return (CgsDev::RGBA)((luR << 24) | (luG << 16) | (luB << 8) | luA);
        }

        inline Vector2 MakeVector2(f32 lfX, f32 lfY)
        {
            Vector2 lv2Result;
            lv2Result.x = lfX;
            lv2Result.y = lfY;
            lv2Result.z = 0.0f;
            lv2Result.w = 0.0f;
            return lv2Result;
        }
    }

    // ------------------------------------------------------------------------
    // RenderPoly @0x82533578
    // Draw a 2D debug box for the projected poly. 1st Draw2DBox vector = vertex 0
    // shifted by the menu anchor; 2nd vector = the V2 - V0 geometry delta (NO menu
    // shift -- the X360 vsubfp128 v127 = Vertices[2] - Vertices[0]). Colour = clamp
    // of colour0 only. lpColour1 / lpLocalWorld are passed for call-shape but the
    // X360 never reads them.
    // ------------------------------------------------------------------------
    void ICERender::RenderPoly(const Poly* lpPoly, const Vector4* lpColour0,
                               const Vector4* /*lpColour1*/, const Matrix4* /*lpLocalWorld*/)
    {
        const Vector3& lrV0 = lpPoly->maVertices[0];
        const Vector3& lrV2 = lpPoly->maVertices[2];

        // 1st Draw2DBox vector: vertex 0 + the ICE menu anchor shift.
        const Vector2 lv2Corner = MakeVector2(lrV0.x + (f32)KI_ICE_MENU_SHIFT_X,
                                              lrV0.y + (f32)KI_ICE_MENU_SHIFT_Y);
        // 2nd Draw2DBox vector: the V2 - V0 delta, with NO menu shift.
        const Vector2 lv2Extent = MakeVector2(lrV2.x - lrV0.x, lrV2.y - lrV0.y);

        // Box colour = clamp(*lpColour0) packed to RGBA. The X360 reads ONLY colour0
        // (the 255.0 it subtracts is the [0,255] clamp upper bound, not a 2nd colour
        // operand); lpColour1 and lpLocalWorld are unused.
        const CgsDev::RGBA lColour = PackColourRGBA(*lpColour0);

        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.Get2dRender().Draw2DBox(lv2Corner, lv2Extent, lColour);
        CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
    }

    // ------------------------------------------------------------------------
    // ScrPrintfArg @0x82533750  (the shared core)
    // vsnprintf the varargs into a 256-byte stack buffer, then draw it as 2D debug
    // text at the menu-shifted position in the given colour.
    // ------------------------------------------------------------------------
    void ICERender::ScrPrintfArg(s32 liX, s32 liY, f32 lfSize, const Vector4* lpColour,
                                 const char* lpcFormat, va_list& lrArgs)
    {
        char lacBuffer[256];
        rw::core::stdc::Vsnprintf(lacBuffer, sizeof(lacBuffer), lpcFormat, lrArgs);

        const Vector2 lv2Position = MakeVector2((f32)(liX + KI_ICE_MENU_SHIFT_X),
                                                (f32)(liY + KI_ICE_MENU_SHIFT_Y));
        const CgsDev::RGBA lColour = PackColourRGBA(*lpColour);

        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.Get2dRender().Draw2DText(lacBuffer, lv2Position, lfSize, lColour);
        CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
    }

    // ------------------------------------------------------------------------
    // ScrPrintf @0x825343D0
    // Vararg front end: white text (255,255,255,255), forward to the core.
    // ------------------------------------------------------------------------
    void ICERender::ScrPrintf(s32 liX, s32 liY, f32 lfSize, const char* lpcFormat, ...)
    {
        Vector4 lWhite;
        lWhite.x = lWhite.y = lWhite.z = lWhite.w = 255.0f;

        va_list lArgs;
        va_start(lArgs, lpcFormat);
        ScrPrintfArg(liX, liY, lfSize, &lWhite, lpcFormat, lArgs);
        va_end(lArgs);
    }

    // ------------------------------------------------------------------------
    // ScrShadowPrintf @0x825342E0
    // Three passes: a black shadow at +1,+1 then -1,-1 (offset shadow at alpha 255,
    // RGB 0), then the white main text at (x,y). Matches the X360 (ScrPrintfArg x3:
    // first two with the dark colour at +/-(1,1), the third white at the true pos).
    // ------------------------------------------------------------------------
    void ICERender::ScrShadowPrintf(s32 liX, s32 liY, f32 lfSize, const char* lpcFormat, ...)
    {
        Vector4 lWhite;
        lWhite.x = lWhite.y = lWhite.z = lWhite.w = 255.0f;

        Vector4 lShadow;          // black, opaque (RGB 0, A 255)
        lShadow.x = lShadow.y = lShadow.z = 0.0f;
        lShadow.w = 255.0f;

        va_list lArgs;

        va_start(lArgs, lpcFormat);
        ScrPrintfArg(liX + 1, liY + 1, lfSize, &lShadow, lpcFormat, lArgs);
        va_end(lArgs);

        va_start(lArgs, lpcFormat);
        ScrPrintfArg(liX - 1, liY - 1, lfSize, &lShadow, lpcFormat, lArgs);
        va_end(lArgs);

        va_start(lArgs, lpcFormat);
        ScrPrintfArg(liX, liY, lfSize, &lWhite, lpcFormat, lArgs);
        va_end(lArgs);
    }
}
