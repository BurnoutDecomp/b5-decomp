#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsErrorWindow.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"           // GetUI().GetPalette()/GetMetrics()/RemoveWindow
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // Debug2DImmediateRender

#include <math.h>   // fabsf

// CgsDev::DebugUI::ErrorWindow - X360 Render (0x8282EF10) / Update (0x828306F0).
//
// Reconstructed store-for-store from the X360 asm. The shared palette colour + the screen metrics
// live in the DebugUI singleton and, following the committed Console.cpp convention, are reached by
// named accessor (GetUI().GetPalette()/GetMetrics()) rather than the X360 raw offsets
// (*(DebugUI+0x50)=mColourErrorWindow, *(DebugUI+0x5C)=Metrics.mfTextSize (Metrics base = +0x5C),
// *(DebugUI+0x94)=Metrics.mfErrorWindowBorder).

namespace CgsDev
{
    namespace DebugUI
    {
        namespace
        {
            // The X360 Window::ScaleColour repack (0x8282EF88..0x8282F044): keep the top (alpha) byte
            // of the packed colour and scale each of the three low channel bytes by lfScale.
            RGBA ScaleColour(RGBA lColour, f32 lfScale)
            {
                const u32 luB0 = (u32)((f32)( lColour        & 0xFFu) * lfScale);
                const u32 luB1 = (u32)((f32)((lColour >>  8) & 0xFFu) * lfScale);
                const u32 luB2 = (u32)((f32)((lColour >> 16) & 0xFFu) * lfScale);
                return (lColour & 0xFF000000u)          // preserve the top (alpha) byte
                     | ((luB2 & 0xFFu) << 16)
                     | ((luB1 & 0xFFu) <<  8)
                     |  (luB0 & 0xFFu);
            }
        }

        void ErrorWindow::Render(Debug2DImmediateRender* lpRender)
        {
            Window::Render(lpRender);

            const Palette& lrPalette = GetUI().GetPalette();
            const Metrics& lrMetrics = GetUI().GetMetrics();

            // Pulse intensity = clamp(|mfPulse|, 0, 255) -> scale = intensity / 255.
            // (asm: fabs, then fsel selects |pulse| when 255-|pulse| >= 0, else 255.0.)
            const f32 lfMagnitude = fabsf(mfPulse);
            const f32 lfIntensity = (255.0f - lfMagnitude) >= 0.0f ? lfMagnitude : 255.0f;
            const f32 lfScale     = lfIntensity * (1.0f / 255.0f);

            // Both the box fill and the frame draw with the pulse-scaled error-window colour.
            const RGBA lPulseColour = ScaleColour(lrPalette.mColourErrorWindow, lfScale);

            // Window geometry (mfX/mfY/mfWidth/mfHeight are private on the base -> public getters).
            const f32 lfX      = GetX();
            const f32 lfY      = GetY();
            const f32 lfWidth  = GetWidth();
            const f32 lfHeight = GetHeight();

            // Fill: origin (mfX,mfY), size (mfWidth,mfHeight).
            lpRender->DrawBox(lfX, lfY, lfWidth, lfHeight, lPulseColour);

            // Frame (X360 sub_8281C960 == DrawFrame 6-arg): rect + pulse colour + the metric border.
            lpRender->DrawFrame(lfX,
                                lfY,
                                lfX + lfWidth,
                                lfY + lfHeight,
                                lPulseColour,
                                lrMetrics.mfErrorWindowBorder);

            // Wrapped error text, inset from the top by the error-window border, drawn at the palette
            // error-window colour (UNSCALED) and text size, centred (lfAlign = 0.5).
            lpRender->DrawTextInBox(mpcErrorMessage,
                                    lfX,
                                    lfY + lrMetrics.mfErrorWindowBorder,
                                    lfX + lfWidth,
                                    lfY + lfHeight,
                                    lrMetrics.mfTextSize,
                                    lrPalette.mColourErrorWindow,
                                    0.5f);
        }

        // X360 0x828306F0. Advance the colour pulse each frame; close on the next confirm/back/close.
        //   - SELECT(1)/BACK(2)/CLOSE(3) removes this window from the stack.
        //   - mfPulse += lfTimeStep * 341.33334 (a 0..512 saw sweep; flt_820DDF2C).
        //   - when it passes 255 it wraps back by 512.
        void ErrorWindow::Update(f32 lfTimeStep, InputEvent leEvent)
        {
            if (leEvent == E_INPUTEVENT_BACK ||
                leEvent == E_INPUTEVENT_SELECT ||
                leEvent == E_INPUTEVENT_CLOSE)
            {
                GetUI().RemoveWindow(this);
            }

            mfPulse += lfTimeStep * 341.33334f;   // flt_820DDF2C
            if (mfPulse > 255.0f)                  // flt_82010C20
                mfPulse -= 512.0f;                 // flt_820DDF28
        }
    }
}
