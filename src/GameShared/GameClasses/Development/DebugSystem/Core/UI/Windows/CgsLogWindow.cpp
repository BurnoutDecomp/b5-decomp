#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"   // the debug operator new[] shim (Construct's line-ring alloc)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <cstring>

// CgsDev::DebugUI::LogWindow / LogWindowStrStream - the default ctor + the stream sink. Recovered
// from the DecFIGS DWARF (Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h) + the X360 default
// ctor at 0x827DFED0.

namespace CgsDev
{
    namespace DebugUI
    {
        // X360 0x827DFED0. The construction stores in the pseudocode are the compiler-emitted base +
        // member construction:
        //   a1[9]  = off_820CE4CC  -> the MenuItem vtable of CustomWindow::mMenuItem (base ctor)
        //   *a1    = off_820CFE94  -> the LogWindow vtable (most-derived ctor)
        //   a1[14] = off_82000D00  -> mLog's StrStreamBase base vtable, then Clear(a1+14) folds
        //                             mLog.mePrintMode = 0  (the StrStreamBase base ctor)
        //   *v2    = off_820CDBC8  -> mLog's LogWindowStrStream vtable (mLog member ctor)
        //   a1[16] = a1            -> mLog.mpWindow = this   (the body below)
        LogWindow::LogWindow()
            : CustomWindow()
            , mLog()
            , mpLinesArray(nullptr)
            , miLineCount(0)
            , miLineHead(0)
            , mfHorizontalIndent(0.0f)
            , mfVerticalIndent(0.0f)
            , mfCurrentWidth(0.0f)
            , mbAutosize(false)
        {
            // The `a1[16] = a1` store: point the embedded stream back at this window.
            mLog.SetWindow(this);
        }

        // operator<<(const char*) sink. COMPILE-REQUIRED override (the base
        // StrStreamBase::operator<<(const char*) is pure-virtual) -- NOT this TU's ledger func and
        // with NO asm in this TU's dossier. Body is a CONSERVATIVE RECONSTRUCTION (not recovered):
        // it forwards each chunk to the owning window's line ring via LogWindow::Append (its own
        // not-yet-done TU); while unbound (mpWindow null) it is a guarded no-op.
        StrStreamBase& LogWindowStrStream::operator<<(const char* lpcText)
        {
            if (mpWindow && lpcText)
                mpWindow->Append(lpcText);
            return *this;
        }

        // @ 0x8281A188 -- size the line ring: store the capacity, allocate maxLines x
        // 60-byte console lines from the debug resource allocator (X360
        // `operator new(60*maxLines, *(DebugInternal::mpInstance+8284), 0)` -- the PC
        // route is the committed CgsDebugCollections operator new[] shim over the same
        // GetAllocator() singleton), default the layout fields (width 100, autosize on,
        // zero indents), zero each line's first byte and reset the head.
        void LogWindow::Construct(s32 liMaxLines)
        {
            miLineCount = static_cast<s8>(liMaxLines);   // +72 (the ring capacity store)
            mpLinesArray = static_cast<CConsoleTextLine*>(
                ::operator new[](sizeof(CConsoleTextLine) * static_cast<size_t>(liMaxLines),
                                 GetAllocator(), Internal::E_ALLOCATION_NORMAL));
            mfCurrentWidth     = 100.0f;   // +84
            mbAutosize         = true;     // +88
            mfHorizontalIndent = 0.0f;     // +76
            mfVerticalIndent   = 0.0f;     // +80
            for (s32 liLine = 0; liLine < liMaxLines; ++liLine)
                mpLinesArray[liLine].macText[0] = 0;
            miLineHead = 0;                // +73
        }

        bool LogWindow::Prepare(const char* lpcCaption, const char* lpcMenuPath, s32 lxFlags)
        {
            Window::Prepare(mfCurrentWidth, ComputeConsoleHeight(), lpcCaption, lxFlags);
            mMenuItem.Prepare(this);
            if (lpcMenuPath)
                Register(lpcMenuPath);
            return true;
        }

        void LogWindow::Update(f32 lfTimeStep, InputEvent leEvent)
        {
            CustomWindow::Update(lfTimeStep, leEvent);
            SetSize(mfCurrentWidth, ComputeConsoleHeight());
            if (leEvent == E_INPUTEVENT_SELECT)
                Clear();
        }

        void LogWindow::Render(Debug2DImmediateRender* lpRender)
        {
            Window::Render(lpRender);

            const Metrics& lrMetrics = GetMetrics();
            RGBA lColour = GetPalette().mColourText;
            if (GetFlags() & KX_FLAGNOBACKGROUND)
                lColour = GetPalette().mColourTextScreen;

            f32 lfY = GetY() + GetHeight() - lrMetrics.mfTextSize;
            s32 liIndex = miLineHead;
            for (s32 liCount = 0; liCount < static_cast<s32>(miLineCount) - 1; ++liCount)
            {
                liIndex = (liIndex + static_cast<s32>(miLineCount) - 1) % static_cast<s32>(miLineCount);
                lpRender->DrawText(mpLinesArray[liIndex].macText,
                                   GetX() + lrMetrics.mfWindowBorderSize + mfHorizontalIndent,
                                   lfY, lrMetrics.mfTextSize, lColour);
                if (lfY < 0.0f)
                    break;
                lfY -= lrMetrics.mfTextSize;
            }
        }

        void LogWindow::Print(const char* lpcText)
        {
            if (!mpLinesArray || miLineCount <= 0 || !lpcText)
                return;

            if (mpLinesArray[miLineHead].macText[0])
            {
                miLineHead = static_cast<s8>((miLineHead + 1) % miLineCount);
                RefreshWidth();
                mpLinesArray[miLineHead].macText[0] = '\0';
            }

            Append(lpcText);

            if (mpLinesArray[miLineHead].macText[0])
            {
                miLineHead = static_cast<s8>((miLineHead + 1) % miLineCount);
                RefreshWidth();
                mpLinesArray[miLineHead].macText[0] = '\0';
            }
        }

        void LogWindow::Append(const char* lpcText)
        {
            if (!mpLinesArray || miLineCount <= 0 || !lpcText)
                return;

            const char* lpcRead = lpcText;
            while (*lpcRead)
            {
                char* lpcLine = mpLinesArray[miLineHead].macText;
                s32 liLength = static_cast<s32>(std::strlen(lpcLine));
                char* lpcWrite = lpcLine + liLength;

                while (*lpcRead && *lpcRead != '\n' && liLength < KI_CONSOLESTRINGLENGTH - 1)
                {
                    *lpcWrite++ = *lpcRead++;
                    ++liLength;
                }
                *lpcWrite = '\0';

                if (!*lpcRead)
                    break;
                if (*lpcRead == '\n')
                    ++lpcRead;

                miLineHead = static_cast<s8>((miLineHead + 1) % miLineCount);
                RefreshWidth();
                mpLinesArray[miLineHead].macText[0] = '\0';
            }
        }

        void LogWindow::Clear()
        {
            for (s32 liIndex = 0; liIndex < miLineCount; ++liIndex)
                mpLinesArray[liIndex].macText[0] = '\0';
            miLineHead = 0;
        }

        f32 LogWindow::ComputeConsoleHeight()
        {
            const Metrics& lrMetrics = GetMetrics();
            f32 lfHeight = static_cast<f32>(miLineCount - 1) * lrMetrics.mfTextSize + mfVerticalIndent;
            if (lfHeight > lrMetrics.mfScreenHeight)
                lfHeight = lrMetrics.mfScreenHeight;
            return lfHeight;
        }

        void LogWindow::RefreshWidth()
        {
            if (!mbAutosize || miLineCount <= 0)
                return;

            const s32 liIndex = (static_cast<s32>(miLineHead) + static_cast<s32>(miLineCount) - 1) %
                                static_cast<s32>(miLineCount);
            const f32 lfWidth = Get2DRenderer()->CalcTextWidth(mpLinesArray[liIndex].macText,
                                                               GetMetrics().mfTextSize) +
                                mfHorizontalIndent;
            if (lfWidth > GetWidth())
                mfCurrentWidth = lfWidth;
            SetSize(mfCurrentWidth, GetHeight());
        }
    }
}
