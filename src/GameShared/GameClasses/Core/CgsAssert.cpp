#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "rw/core/debug/DebugCriticalSection.h"   // the canonical wrapper (this TU shares its home)

// The assert mutex is a rw::core::debug::detail::DebugCriticalSection (its Enter/Leave wrap a
// Win32 critical section, gated by the "initialised" flag - an un-Created section is a no-op,
// matching the X360 "if (*result)" guard at 0x82BBC4D8/0x82BBC4F0). gAssertMutex stays
// un-Created on the single-threaded boot (Begin/EndAssert become no-ops); the threading core
// Creates it.

namespace CgsDev
{
namespace Assert
{
    static rw::core::debug::detail::DebugCriticalSection gAssertMutex = { 0 };

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

    // @ 0x82820810 - report a fired assertion. The X360 prepares an X360 stack trace
    // (StackUnpickX360::Prepare) then forwards to Manager::HandleAssert. Stack capture is skipped
    // on PC; the manager logs the failure to BrnGame_asserts.log. The debug break is left
    // commented out so asserts log and continue rather than halting.
    int FireAssert(const char* lpcExpression, const char* lpcFile, int liLine)
    {
        // CgsDev::StackUnpickX360::Prepare(...);   // X360 stack capture - no PC equivalent here
        gAssertManager.HandleAssert(lpcExpression, lpcFile, liLine);
        // __debugbreak();   // commented out per request - asserts log to file and continue
        return 0;
    }
}
}
