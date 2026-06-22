#ifndef RW_CORE_DEBUG_DEBUGCRITICALSECTION_H
#define RW_CORE_DEBUG_DEBUGCRITICALSECTION_H

#include "types.hpp"

// This header is pulled in transitively by rw/rwcore_structs.h (the rw:: type vocabulary), so it is
// included very widely. Keep <windows.h> from leaking its USER/GDI macros (GetNextWindow, DrawText,
// GetUI...) into the project's debug-UI/render code: CRITICAL_SECTION lives in winbase.h, which the
// LEAN_AND_MEAN subset keeps while NOGDI/NOUSER/NOMINMAX drop the colliding macros.
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#  define NOGDI
#endif
#ifndef NOUSER
#  define NOUSER
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

// rw::core::debug::detail::DebugCriticalSection - RenderWare's thin wrapper around a Win32
// critical section, gated by an "initialised" flag so an un-Created section is a no-op.
//
// X360 layout (attested by Create/Enter/Leave @ 0x82BBC4A0 / 0x82BBC4D8 / 0x82BBC4F0):
//   +0x00  miInitialised   - set to 1 by Create; Enter/Leave are no-ops while it is 0.
//   +0x04  mCriticalSection - the OS CRITICAL_SECTION (Create passes &this->mCriticalSection,
//                             i.e. this+4, to RtlInitializeCriticalSection; the pseudocode's
//                             "a1 + 1" is +4 bytes on a _DWORD*).
//
// Create wraps RtlInitializeCriticalSection (the Win32 InitializeCriticalSection); Enter/Leave
// wrap RtlEnter/LeaveCriticalSection. On PC these are the documented Win32 entry points.
// The debug manager owns one of these (the per-DebugManager lock DebugInterface acquires) and
// the assert front-end owns another (gAssertMutex). Both stay un-Created (no-op) on the
// single-threaded boot; the threading core Creates them.

namespace rw { namespace core { namespace debug { namespace detail {

    struct DebugCriticalSection
    {
        // @ 0x82BBC4A0 - initialise the OS critical section and mark the wrapper enabled.
        int   Create();
        // @ 0x82BBC4D8 - enter the section if it has been Created (else a no-op).
        void* Enter();
        // @ 0x82BBC4F0 - leave the section if it has been Created (else a no-op).
        void* Leave();

        int              miInitialised;
        CRITICAL_SECTION mCriticalSection;
    };

}}}}

#endif // RW_CORE_DEBUG_DEBUGCRITICALSECTION_H
