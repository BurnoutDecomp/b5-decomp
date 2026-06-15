#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <windows.h>

namespace CgsDev
{
namespace Assert
{
    Manager gAssertManager;

    Manager::Manager()
        : miAssertCount(0)
    {
    }

    // @ CgsAssertManager.h:150 / DoAssert 0x82820338 - the X360 path dumps the callstack via the
    // debug log (gpDebugPrint), runs the registered handlers, interrupts the other threads, then
    // loops forever in DisplayAssertScreen (the on-screen assert dialog). Adapted per the build
    // config: log the failure + callstack to the unified game log (BrnGame.log) via gpDebugPrint,
    // run any handlers, and CONTINUE (no halt). Asserts log unconditionally (not gated by the
    // message filter).
    void Manager::HandleAssert(const char* lpcExpression, const char* lpcFile, s32 liLine)
    {
        if (!lpcExpression)
            lpcExpression = "<no expression>";
        if (!lpcFile)
            lpcFile = "<no file>";
        ++miAssertCount;

        // Callstack (PC equivalent of StackUnpickX360::Prepare's DmCaptureStackBackTrace +
        // DoAssert's callstack dump; map-file symbol resolution is the on-screen-dialog's job,
        // so frames are logged as raw addresses).
        void* lapStack[24];
        const USHORT luFrames = CaptureStackBackTrace(1, 24, lapStack, 0);

        Log::DebugPrint* lpLog = Log::gpDebugPrint;
        *lpLog << "[ASSERT " << miAssertCount << "] " << lpcExpression
               << " (" << lpcFile << ":" << liLine << ")\n";
        *lpLog << "  Callstack:\n";
        for (USHORT i = 0; i < luFrames; ++i)
            *lpLog << "    " << lapStack[i] << "\n";
        *lpLog << "  EndCallstack\n";

        ExecuteAssertHandlers();
        // DoAssert(): X360 path also runs InteruptThreadForAssert + DisplayAssertScreen (halts)
        // - replaced by the log above so the build logs and continues.
    }

    void Manager::RegisterAssertHandler(AssertHandler*)
    {
        // Handler registration is reconstructed with the assert-handler interface; the boot
        // path registers none.
    }

    void Manager::ExecuteAssertHandlers()
    {
        // No handlers registered on the boot path.
    }
}
}
