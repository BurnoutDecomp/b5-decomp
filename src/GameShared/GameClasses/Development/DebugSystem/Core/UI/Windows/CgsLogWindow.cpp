#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"   // the debug operator new[] shim (Construct's line-ring alloc)

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

        // FLAG: MINIMAL STUBS FOR LINK (not decompiled). The LogWindow render/update protocol + the
        // line-ring push are grown when the DebugUI window stack lands; Append is a guarded no-op
        // (the ring buffer mpLinesArray is unallocated in this minimal slice).
        void LogWindow::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/) {}
        void LogWindow::Render(Debug2DImmediateRender* /*lpRender*/) {}
        void LogWindow::Append(const char* /*lpcText*/) {}
    }
}
