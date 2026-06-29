#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Console/CgsConsole.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"      // GetUI().GetMetrics()/AddWindow/RemoveWindow
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"    // DebugManagerConstructParameters

// CgsDev::DebugUI::Console - the drop-down debug console. Reconstructed from the X360 (Construct
// 0x8282E6B0, ToggleShow 0x828292A0, Update 0x8282FC68).
//
// The X360 reaches the screen metrics + the window stack through the debug singleton
// (mpInstance->mpUI->...): the source reads the UI's Metrics sub-object at raw offsets
// (mfTextSize @ +0x5C as the per-line height, mfScreenWidth @ +0x68, mfScreenHeight @ +0x6C,
// mfScreenBorderLeft @ +0x70, mfScreenBorderTop @ +0x74) and walks the window list at +4. Following
// the subsystem convention, those are reconstructed by name through GetUI().GetMetrics() and the
// AddWindow/RemoveWindow/IsWindowAdded accessors (semantic parity, not the raw offsets).

namespace CgsDev
{
    namespace DebugUI
    {
        // The vertical slide rate Update inlines as the literal 4.0 (flt_82004EF4). KF_TOGGLETIME is
        // its reciprocal (see header) - INFERRED, since neither value is read by name in this TU.
        const f32 Console::KF_SLIDESPEED = 4.0f;
        const f32 Console::KF_TOGGLETIME = 0.25f;

        Console::Console()
            : LogWindow()
            , mbVisible(false)
            , mbFullyVisible(false)
            , mbEnabled(false)
        {
        }

        // X360 0x8282E6B0. Build the line ring from the construct parameters, clear every stored line,
        // set invisible + enabled, then prepare the window: full screen width, height = (lines-1) per
        // line height + the top border, capped at the screen height; docked just above the top edge
        // (y = -height). The window is NOCAPTION|NOFOCUS|NOCASCADE|NOCLAMPTOSCREEN (flags 0x342).
        void Console::Construct(const DebugManagerConstructParameters* lpParameters)
        {
            LogWindow::Construct(static_cast<s32>(lpParameters->miConsoleLineCount));

            // Clear the first byte of each stored line (null-terminate the whole ring).
            for (s32 liIndex = 0; liIndex < miLineCount; ++liIndex)
                mpLinesArray[liIndex].macText[0] = 0;

            miLineHead     = 0;
            mbVisible      = false;
            mbFullyVisible = false;
            mbAutosize     = false;
            mbEnabled      = true;

            const Metrics& lrMetrics = GetUI().GetMetrics();

            // The X360 stores the per-line height + top border into the LogWindow indent fields.
            mfHorizontalIndent = lrMetrics.mfScreenBorderLeft;   // metrics @ +0x70
            mfVerticalIndent   = lrMetrics.mfScreenBorderTop;    // metrics @ +0x74

            // height = (lineCount - 1) * lineHeight + topBorder, clamped to the screen height.
            f32 lfHeight = static_cast<f32>(miLineCount - 1) * lrMetrics.mfTextSize + lrMetrics.mfScreenBorderTop;
            if (lfHeight > lrMetrics.mfScreenHeight)
                lfHeight = lrMetrics.mfScreenHeight;

            Window::Prepare(lrMetrics.mfScreenWidth, lfHeight, /*lpcCaption*/ 0,
                            Window::KX_FLAGNOCAPTION | Window::KX_FLAGNOFOCUS |
                            Window::KX_FLAGNOCASCADE | Window::KX_FLAGNOCLAMPTOSCREEN);

            // Dock just above the top of the screen (slid fully out of view to begin with).
            SetPosition(0.0f, -lfHeight);
            Window::ClampToScreen();
        }

        // X360 0x8282FC68. Re-measure against the current line count, then step the slide animation.
        void Console::Update(f32 lfTimeStep, InputEvent /*leEvent*/)
        {
            const Metrics& lrMetrics = GetUI().GetMetrics();

            // Current height = (lineCount - 1) * lineHeight + the stored top border, capped at screen.
            f32 lfHeight = static_cast<f32>(miLineCount - 1) * lrMetrics.mfTextSize + mfVerticalIndent;
            if (lfHeight > lrMetrics.mfScreenHeight)
                lfHeight = lrMetrics.mfScreenHeight;

            SetSize(lrMetrics.mfScreenWidth, lfHeight);

            // Animate only while the visible target and the settled flag disagree.
            if (mbVisible != mbFullyVisible)
            {
                const f32 lfStep = lfHeight * KF_SLIDESPEED * lfTimeStep;
                f32       lfY    = GetY();

                if (mbVisible)
                {
                    // Slide down into view; clamp at the top edge (y = 0) and mark settled.
                    lfY += lfStep;
                    if (lfY >= 0.0f)
                    {
                        lfY            = 0.0f;
                        mbFullyVisible = true;
                    }
                }
                else
                {
                    // Slide back up out of view; clamp at fully-hidden (y = -height), mark settled,
                    // and drop out of the window stack.
                    const f32 lfHidden = -lfHeight;
                    lfY -= lfStep;
                    if (lfY <= lfHidden)
                    {
                        lfY            = lfHidden;
                        mbFullyVisible = false;
                        GetUI().RemoveWindow(this);
                    }
                }

                SetPosition(GetX(), lfY);
                Window::ClampToScreen();
            }
        }

        // X360 0x828292A0. Toggle visibility while enabled, and ensure the console is in the window
        // stack so Update is ticked.
        void Console::ToggleShow()
        {
            if (!mbEnabled)
                return;

            mbFullyVisible = mbVisible;   // the settled flag now lags the new target
            mbVisible      = !mbVisible;

            if (!GetUI().IsWindowAdded(this))
                GetUI().AddWindow(this);
        }

        // X360 CgsConsole.cpp:93 - the line-ring teardown rides on the LogWindow follow-on.
        void Console::Destruct() {}
    }
}
