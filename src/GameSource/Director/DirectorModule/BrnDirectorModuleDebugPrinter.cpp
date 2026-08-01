#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"

#include "BrnCommonTypes.h"                                                        // Vector2
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"          // CgsDev::DebugManager::ThreadSafeRelease
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"            // CgsDev::DebugUI::DebugUI::GetMetrics
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"             // CgsDev::DebugUI::Metrics
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"        // CgsDev::DebugRender
#include "GameSource/Director/MomentController/BrnMoment.h"                             // BrnDirector::Moment (PrintName virtual GetName dispatch)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                 // CgsCore::SnPrintf (DebugLog::ActualAppend)

// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.cpp
//
// The Director's on-screen debug-text helpers (DebugPrinter / DebugLog). Reconstructed from the
// X360 ARTIST build for behaviour and the DecFIGS DWARF for declaration shape:
//   DebugPrinter::Construct   @ 0x821F7108
//   DebugPrinter::ActualPrint @ 0x821F71D8
//   DebugLog::Print           @ 0x8221BAC8
// Each method that touches the debug system brackets its work with a stack-scoped
// CgsDev::DebugInterface (acquire) + an explicit DebugManager::ThreadSafeRelease (release), the
// established Director idiom (see BrnDirectorDevTools.cpp) -- the asm makes the release conditional
// on the interface's "automatic" flag, which the default DebugInterface() ctor always sets.
// ============================================================================

namespace BrnDirector
{
    // X360 0x821F7108. Seed the printer's render parameters from the debug-UI screen Metrics and a
    // fixed colour palette, then enable it. The asm reads the UI Metrics block by an inlined copy
    // (memcpy of all 18 floats) and uses two of them: position = (screenWidth*0.25, screenHeight*0.1).
    void DebugPrinter::Construct()
    {
        CgsDev::DebugInterface lDebugInterface;

        const CgsDev::DebugUI::Metrics lMetrics = lDebugInterface.GetUI().GetMetrics();

        // Fixed cursor metrics + palette (constants verbatim from the asm).
        mDebugPrinterInfo.mfSize          = 16.0f;   // flt_82004000
        mDebugPrinterInfo.mfLineSize      = 17.6f;   // flt_82004174
        mDebugPrinterInfo.mfX             = lMetrics.mfScreenWidth  * 0.25f;  // flt_82003F40
        mDebugPrinterInfo.mfY             = lMetrics.mfScreenHeight * 0.1f;   // flt_82004014
        mDebugPrinterInfo.muColour         = 0xFFFFFFFF;  // white
        mDebugPrinterInfo.muActiveColour   = 0xFF00FF00;  // green
        mDebugPrinterInfo.muInactiveColour = 0xFF0000FF;  // red
        mDebugPrinterInfo.mJustification   = CgsDev::DebugRender::E_JUSTIFY_LEFT;
        mbEnabled = true;

        CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
    }

    // X360 0x821F71D8. If enabled, draw one justified line at the current cursor through the buffered
    // 2D debug renderer, then advance the Y cursor down by one line height.
    void DebugPrinter::ActualPrint(const char* lpcMessage, CgsDev::RGBA luColour)
    {
        if (!mbEnabled)
        {
            return;
        }

        CgsDev::DebugInterface lDebugInterface;

        const Vector2 lv2Position = { mDebugPrinterInfo.mfX, mDebugPrinterInfo.mfY, 0.0f, 0.0f };

        lDebugInterface.Get2dRender().Draw2DTextJustified(
            lpcMessage,
            lv2Position,
            mDebugPrinterInfo.mJustification,
            mDebugPrinterInfo.mfSize,
            luColour);

        // Advance the cursor to the next line.
        mDebugPrinterInfo.mfY += mDebugPrinterInfo.mfLineSize;

        CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
    }

    // X360 0x82218D50 (class:BrnDirector::DebugPrinter TU). Resolve the moment's display
    // name through its live vtable (slot 6, `lwz r11,0x18(vtbl); bctrl`) and draw it.
    void DebugPrinter::PrintName(const Moment& lrMoment, CgsDev::RGBA luColour)
    {
        ActualPrint(lrMoment.GetName(), luColour);
    }

    // X360 0x8221BB78. Take a pool slot for one new log line -- recycling the OLDEST live line
    // when the pool is full -- copy the text in, stamp it with the log's on-screen lifetime and
    // colour, and push its slot index onto the live-order array.
    //
    // The recycle path is the whole point of the body: AllocateObject returns -1 on a full pool,
    // and the console then frees the slot named by mStringIndices[0] (the oldest entry, since
    // Append pushes at the back), erases that index, and retries. The second failure is a hard
    // assert -- the console's own, with its file/line
    // (GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.cpp:221, which is what
    // pins this body to THIS TU) -- and, note, it does NOT return: it falls through and writes
    // through the -1 index, so a dev build that clicks past the assert is already broken. That is
    // reproduced as written; adding a guard here would be a silent behaviour change.
    //
    // The text copy is CgsCore::SnPrintf(dst, 64, "%s", src) -- the format-through-"%s" idiom, not
    // a StringnCopy -- followed by an explicit `macString[63] = 0` (the console's `stb r10, 0x47(p)`;
    // macString sits at +0x08 of the 72-byte LogString, so 0x47 is its last byte).
    void DebugLog::ActualAppend(const char* lpcString, CgsDev::RGBA lRGBA)
    {
        s32 liNewIndex = mStringPool.AllocateObject();
        if (liNewIndex == -1)
        {
            // Pool full: recycle the oldest live line (front of the live-order array).
            mStringPool.FreeObject(mStringIndices.GetItem(0));
            mStringIndices.Erase(0);

            liNewIndex = mStringPool.AllocateObject();
            CGS_ASSERT(liNewIndex != -1, "liNewIndex != -1");
        }

        LogString& lrString = mStringPool[liNewIndex];
        CgsCore::SnPrintf(lrString.macString, sizeof(lrString.macString), "%s", lpcString);
        lrString.macString[sizeof(lrString.macString) - 1] = '\0';
        lrString.mfTimeLeft = mfStringDuration;
        lrString.mRGBA      = lRGBA;

        mStringIndices.Append(liNewIndex);
    }

    // X360 0x8224EE00 (class:BrnDirector::DebugLog TU). Resolve the moment's display
    // name through its live vtable (slot 6, `lwz r11,0x18(vtbl); bctrl`) and append it.
    void DebugLog::AppendName(const Moment& lrMoment, CgsDev::RGBA lRGBA)
    {
        ActualAppend(lrMoment.GetName(), lRGBA);
    }

    // X360 0x8221BAC8. Replay every live pool entry, in mStringIndices order, through the printer's
    // private ActualPrint(text, colour) path.
    void DebugLog::Print(DebugPrinter& lrDebugPrinter)
    {
        for (u32 luLoop = 0; luLoop < mStringIndices.GetLength(); ++luLoop)
        {
            const s32       liIndex   = mStringIndices[luLoop];
            const LogString& lrString = mStringPool[liIndex];
            lrDebugPrinter.ActualPrint(lrString.macString, lrString.mRGBA);
        }
    }
}
