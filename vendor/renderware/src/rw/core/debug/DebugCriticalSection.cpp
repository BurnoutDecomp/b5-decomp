#include "rw/core/debug/DebugCriticalSection.h"

// rw::core::debug::detail::DebugCriticalSection - store-for-store ports of the X360 wrappers.
// The X360 calls the Rtl* critical-section entry points; on PC these are the InitializeCriticalSection /
// EnterCriticalSection / LeaveCriticalSection Win32 APIs (same routines, the Rtl* names are the
// ntdll-level aliases the X360 linked against).

namespace rw { namespace core { namespace debug { namespace detail {

    // ARTIST 0x82BBC4A0. The XDK initializer and the DecFIGS declaration return void.
    void DebugCriticalSection::Create()
    {
        InitializeCriticalSection(&mCriticalSection);
        miInitialised = 1;
    }

    // ARTIST 0x82BBC4D8.
    void DebugCriticalSection::Enter()
    {
        if (miInitialised)
            EnterCriticalSection(&mCriticalSection);
    }

    // ARTIST 0x82BBC4F0.
    void DebugCriticalSection::Leave()
    {
        if (miInitialised)
            LeaveCriticalSection(&mCriticalSection);
    }

}}}}
