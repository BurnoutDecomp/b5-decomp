#include "rw/core/debug/DebugCriticalSection.h"

// rw::core::debug::detail::DebugCriticalSection - store-for-store ports of the X360 wrappers.
// The X360 calls the Rtl* critical-section entry points; on PC these are the InitializeCriticalSection /
// EnterCriticalSection / LeaveCriticalSection Win32 APIs (same routines, the Rtl* names are the
// ntdll-level aliases the X360 linked against).

namespace rw { namespace core { namespace debug { namespace detail {

    // X360 0x82BBC4A0:
    //   result = RtlInitializeCriticalSection(this + 4);   // this->mCriticalSection
    //   *this = 1;                                         // this->miInitialised
    //   return result;
    int DebugCriticalSection::Create()
    {
        InitializeCriticalSection(&mCriticalSection);
        miInitialised = 1;
        return 0;
    }

    // X360 0x82BBC4D8:
    //   if (*this) return RtlEnterCriticalSection(this + 4);
    //   return this;
    void* DebugCriticalSection::Enter()
    {
        if (miInitialised)
            EnterCriticalSection(&mCriticalSection);
        return this;
    }

    // X360 0x82BBC4F0:
    //   if (*this) return RtlLeaveCriticalSection(this + 4);
    //   return this;
    void* DebugCriticalSection::Leave()
    {
        if (miInitialised)
            LeaveCriticalSection(&mCriticalSection);
        return this;
    }

}}}}
