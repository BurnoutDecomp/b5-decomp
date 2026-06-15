#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"

#include <windows.h>

// rw::core::debug::detail::DebugCriticalSection - the assert mutex wrapper. Enter/Leave wrap a
// Win32 critical section, gated by an "enabled" flag (disabled = no-op, matching the X360
// "if (*result)" guard at 0x82BBC4D8/0x82BBC4F0). gAssertMutex stays disabled on the
// single-threaded boot (Begin/EndAssert become no-ops); the threading core enables it.
namespace rw { namespace core { namespace debug { namespace detail {
    struct DebugCriticalSection
    {
        int              miEnabled;
        CRITICAL_SECTION mCriticalSection;

        static void* Enter(DebugCriticalSection* lpThis)
        {
            if (lpThis->miEnabled)
                EnterCriticalSection(&lpThis->mCriticalSection);
            return lpThis;
        }
        static void* Leave(DebugCriticalSection* lpThis)
        {
            if (lpThis->miEnabled)
                LeaveCriticalSection(&lpThis->mCriticalSection);
            return lpThis;
        }
    };
}}}}

namespace CgsDev
{
namespace Assert
{
    static rw::core::debug::detail::DebugCriticalSection gAssertMutex = { 0 };

    // @ 0x82817548 - enter the assert mutex.
    int BeginAssert()
    {
        rw::core::debug::detail::DebugCriticalSection::Enter(&gAssertMutex);
        return 0;
    }

    // @ 0x82817558 - leave the assert mutex.
    void* EndAssert()
    {
        return rw::core::debug::detail::DebugCriticalSection::Leave(&gAssertMutex);
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
