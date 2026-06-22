#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsCustomWindow.h"

// CgsDev::DebugUI::LogWindow - a CustomWindow that displays a scrolling ring of recent log lines.
// Text is streamed in through its embedded LogWindowStrStream (`logWindow.mLog << ...`), whose char*
// sink (LogWindowStrStream::Append) pushes a line into the window's CConsoleTextLine ring. Recovered
// from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h) + the X360 default
// ctor at 0x827DFED0. Constructed by BrnSound::Debug::DebugComponent::DebugComponent.
//
// Layout (X360, from DWARF + ctor word indices): CustomWindow (Window + CustomWindowMenuItem == 56B)
// then mLog @ +56 (the LogWindowStrStream: StrStreamBase 8B + mpWindow 4B), so mLog.mpWindow lands at
// +64 (word 16) - exactly the `a1[16] = a1` store in the ctor. The line ring + indents follow.

namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        struct LogWindow;

        // The stream front-end embedded in a LogWindow: its char* sink forwards each chunk of text to
        // the owning window's line ring. mpWindow points back at that LogWindow.
        struct LogWindowStrStream : public StrStreamBase
        {
            LogWindowStrStream() : mpWindow(nullptr) {}

            // The sink (X360 CgsLogWindow.cpp:56): hand the text to mpWindow->Append. operator<<(const
            // char*) forwards here; while unbound (the freshly default-constructed state this TU's
            // LogWindow ctor leaves it in, before mpWindow is wired) it is a no-op.
            StrStreamBase& operator<<(const char* lpcText) override;

            // Wire the back-pointer to the owning window (the ctor's `mLog.mpWindow = this`).
            void SetWindow(LogWindow* lpWindow) { mpWindow = lpWindow; }

        protected:
            LogWindow* mpWindow;   // the LogWindow whose ring this stream feeds
        };

        struct LogWindow : public CustomWindow
        {
            // X360 CgsLogWindow.h:90 - max characters stored per console line.
            static const s32 KI_CONSOLESTRINGLENGTH = 60;

            // One stored console line (X360 CgsLogWindow.h:94).
            struct CConsoleTextLine
            {
                char macText[KI_CONSOLESTRINGLENGTH];
            };

            // X360 0x827DFED0. Default-construct an empty log window: the CustomWindow base + the
            // embedded LogWindowStrStream (mLog) construct first (installing their vtables and clearing
            // mLog.mePrintMode - the `BasePriorityQueue::Clear(a1+14)` the decompiler attributes is the
            // folded mePrintMode=0 store done by the StrStreamBase base ctor of mLog), then the ctor
            // wires mLog back to this window (the `a1[16] = a1` store). The line ring + layout fields
            // start empty.
            LogWindow();

            void Construct(s32 liMaxLines);
            bool Prepare(const char* lpcCaption, const char* lpcMenuPath, s32 lxFlags);

            virtual void Update(f32 lfTimeStep, InputEvent leEvent) override;
            virtual void Render(Debug2DImmediateRender* lpRender) override;

            void Print(const char* lpcText);
            void Append(const char* lpcText);
            void Clear();

        protected:
            f32  ComputeConsoleHeight();
            void RefreshWidth();

            LogWindowStrStream mLog;                  // +56: the stream front-end (mLog.mpWindow @ +64)
            CConsoleTextLine*  mpLinesArray;          // the ring of stored lines
            s8                 miLineCount;
            s8                 miLineHead;
            f32                mfHorizontalIndent;
            f32                mfVerticalIndent;
            f32                mfCurrentWidth;
            bool               mbAutosize;
        };
    }
}
