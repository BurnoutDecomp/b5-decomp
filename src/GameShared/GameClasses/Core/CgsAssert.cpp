#include <xtl.h>
#undef DrawText
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint (the assert print stream)
#include "rw/core/debug/DebugCriticalSection.h"   // the canonical wrapper (this TU shares its home)

// The assert mutex is a rw::core::debug::detail::DebugCriticalSection (its Enter/Leave wrap a
// Win32 critical section, gated by the "initialised" flag - an un-Created section is a no-op,
// matching the X360 "if (*result)" guard at 0x82BBC4D8/0x82BBC4F0). gAssertMutex stays
// un-Created on the single-threaded boot (Begin/EndAssert become no-ops); the threading core
// Creates it.

namespace CgsDev
{
namespace Log
{
    // The stream PrintStringed writes to. The console aims it at its own
    // LogOutput (gAssertPrint), whose sink is the debug output; every log sink in this tree is
    // the unified game log, which is what the debug-print stream writes, so it is that stream.
    StrStreamBase* gpAssertPrint = gpDebugPrint;
}

namespace Assert
{
    namespace
    {
        char gacMessageBuffer[KI_MESSAGEBUFFERSIZE] = {};
    }
    char* gpcMessageBuffer = gacMessageBuffer;
    s32 giMessageBufferSize = KI_MESSAGEBUFFERSIZE;

    static rw::core::debug::detail::DebugCriticalSection gAssertMutex = { 0 };
    namespace
    {
 // protected by gPendingLockPC
   // owner thread only

    }

    // @ 0x82820758 - the once-guarded assert-system bring-up (byte_83019200 latch), called first
    // by DebugManager::Construct: Create gAssertMutex, then initialise the assert-screen state.
    // In this tree the screen-state/vector-font members the X360 body seeds ({-1,0,0} + the
    // {14,14}*0.125 font-size vector at Manager+480) are initialised by Manager's ctor and
    // SetRenderer; Manager::RenderThreadAsserts (the per-thread assert render kick) is the
    // threading follow-on.
    void Construct()
    {
        static bool sbConstructed = false;   // X360 byte_83019200
        if (sbConstructed)
            return;
        sbConstructed = true;

        gAssertMutex.Create();
    }

    // @ 0x82817548 - enter the assert mutex.
    int BeginAssert()
    {
        gAssertMutex.Enter();
        return 0;
    }

    // @ 0x82817558 - leave the assert mutex.
    void* EndAssert()
    {
        return gAssertMutex.Leave();
    }

    // @ 0x82820810 - report a fired assertion. Native workers capture their
    // own stack before handoff; the owner logs it and can display a modal frame.
    int FireAssert(const char* lpcExpression, const char* lpcFile, int liLine)
    {
        gAssertManager.HandleAssert(lpcExpression, lpcFile, liLine);
        // __debugbreak();   // commented out per request - asserts log to file and continue
        return 0;
    }

    // The job-side assert report: one line on the assert print stream,
    // "CGSASSERT : <file>:<line> <message>", with "<NULLSTRING>" for a null file or message.
    // The console never reads lpcFunction.
    void PrintStringed(const char* lpcFile, const char* lpcFunction, const int liLine, const char* lpcMessage)
    {
        (void)lpcFunction;
        StrStreamBase& lrStream = *Log::gpAssertPrint;
        lrStream << "CGSASSERT : ";
        lrStream << (lpcFile ? lpcFile : "<NULLSTRING>");
        lrStream << ":";
        lrStream << static_cast<s32>(liLine) << " " << (lpcMessage ? lpcMessage : "<NULLSTRING>") << "\n";
    }
}
}
