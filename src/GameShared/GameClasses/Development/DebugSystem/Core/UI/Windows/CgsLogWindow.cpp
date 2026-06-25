#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"

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

        // FLAG: MINIMAL STUBS FOR LINK (not decompiled). The LogWindow render/update protocol + the
        // line-ring push are grown when the DebugUI window stack lands; Append is a guarded no-op
        // (the ring buffer mpLinesArray is unallocated in this minimal slice).
        void LogWindow::Update(f32 /*lfTimeStep*/, InputEvent /*leEvent*/) {}
        void LogWindow::Render(Debug2DImmediateRender* /*lpRender*/) {}
        void LogWindow::Append(const char* /*lpcText*/) {}
    }
}
