#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

// CgsDev::DebugUI::ErrorWindow - the pulsing modal debug error box. When the debug system hits an error
// it pushes an ErrorWindow onto the window stack (DebugUI::ShowErrorMessage); the window draws a pulsing
// coloured box + frame behind a wrapped error message and closes itself on the next SELECT/BACK/CLOSE
// input. Recovered from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Windows/CgsErrorWindow.h) +
// the X360 Render (0x8282EF10) / Update (0x828306F0).
//
// Layout (X360, from DWARF + the Render/Update word indices): Window base (vptr@0, mpcCaption@4,
// mxFlags@8, mfTransparency@12, mfX@16, mfY@20, mfWidth@24, mfHeight@28, next@32 == 36B) then the two
// ErrorWindow fields: mpcErrorMessage @ +36 (the *(a1+0x24) read in Render/DrawTextInBox) and mfPulse @
// +40 (the *(a1+0x28) fabs in Render and the *(this+0x28) sweep in Update).
//
// NOTE: Render reads a shared palette colour (mColourErrorWindow) + two metrics (mfTextSize,
// mfErrorWindowBorder) from the DebugUI singleton, reached by named GetUI().GetPalette()/GetMetrics()
// accessors (the committed Console.cpp convention), plus the renderer DrawFrame(6-arg)/DrawTextInBox(8-arg).

namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        struct ErrorWindow : public Window
        {
            // X360 CgsErrorWindow.cpp:35-38 (.rdata float pool). Consumed by ErrorWindow::Prepare (its
            // own TU); Render/Update do not read them.
            static const f32 KF_ERRORWINDOWWIDTH;    // CgsErrorWindow.cpp:35
            static const f32 KF_ERRORWINDOWHEIGHT;   // CgsErrorWindow.cpp:36
            static const f32 KF_ERRORPULSETIME;      // CgsErrorWindow.cpp:38

            ErrorWindow();

            void Prepare(const char* lpcErrorMessage);

            virtual void Update(f32 lfTimeStep, InputEvent leEvent) override;
            virtual void Render(Debug2DImmediateRender* lpRender) override;

        protected:
            const char* mpcErrorMessage;   // +36: the error text drawn in the box (X360 CgsErrorWindow.h:65)
            f32         mfPulse;           // +40: 0..512 saw sweep driving the box-colour pulse (h:66)
        };
    }
}
