#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"

// CgsDev::DebugUI::Console - the drop-down debug console: a LogWindow that slides down from the top
// of the screen on toggle and scrolls back up when dismissed. It is a docked, no-caption,
// no-cascade, no-focus window (the X360 prepares it with flags 0x342 = NOCAPTION|NOFOCUS|NOCASCADE|
// NOCLAMPTOSCREEN) sized to the full screen width and a height that grows with its stored line count
// (capped at the screen height). ToggleShow flips its visibility and adds it to the window stack;
// Update animates the vertical slide and removes it from the stack once it has fully retracted.
// Recovered from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Console/CgsConsole.h) + the X360
// (Construct 0x8282E6B0, ToggleShow 0x828292A0, Update 0x8282FC68).
//
// Layout: it adds nothing to LogWindow's storage but three booleans (DWARF CgsConsole.h:79-81),
// landing at the LogWindow tail: mbVisible @ +0x5C, mbFullyVisible @ +0x5D, mbEnabled @ +0x5E -
// exactly the byte5C/byte5D/byte5E the X360 accesses by raw offset.

namespace CgsDev
{
    struct Debug2DImmediateRender;
    struct DebugManagerConstructParameters;

    namespace DebugUI
    {
        struct Console : public LogWindow
        {
            Console();

            // X360 0x8282E6B0. Build the underlying LogWindow line ring from the construct
            // parameters' console-line count, clear each stored line, set the console invisible +
            // enabled, then prepare the window full-width / line-count-tall (capped at screen height)
            // docked just above the top of the screen.
            void Construct(const DebugManagerConstructParameters* lpParameters);

            // X360 (Destruct, CgsConsole.cpp:93). Not present in this TU's reconstructed slice; the
            // line-ring teardown rides on the LogWindow follow-on.
            void Destruct();

            // X360 0x8282FC68. Resize to the current line count, then advance the slide animation
            // toward the visible/hidden target; when fully retracted, drop out of the window stack.
            virtual void Update(f32 lfTimeStep, InputEvent leEvent) override;

            // X360 0x828292A0. Flip visibility (and add to the window stack if it is not already in
            // it) - but only while the console is enabled.
            void ToggleShow();

            bool IsVisible() const  { return mbVisible; }
            void Enable()           { mbEnabled = true; }
            void Disable()          { mbEnabled = false; }
            bool IsEnabled() const  { return mbEnabled; }

        private:
            // The slide animation rate the X360 inlines as the literal 4.0 in Update (vertical
            // velocity = consoleHeight * KF_SLIDESPEED). The DWARF lists a KF_TOGGLETIME constant for
            // this class (CgsConsole.h:76); it is the reciprocal of this recovered rate (a 0.25s
            // toggle). KF_TOGGLETIME itself is not read by these three functions (the rate is inlined),
            // so its exact stored value is INFERRED from the recovered 4.0, not directly recovered.
            static const f32 KF_SLIDESPEED;   // 4.0 (recovered, inlined in Update @ flt_82004EF4)
            static const f32 KF_TOGGLETIME;   // 0.25 (inferred = 1 / KF_SLIDESPEED)

            bool mbVisible;        // +0x5C: target/current shown state (toggled by ToggleShow)
            bool mbFullyVisible;   // +0x5D: animation-settled flag (tracks mbVisible once the slide ends)
            bool mbEnabled;        // +0x5E: master enable - ToggleShow is a no-op while false
        };
    }
}
