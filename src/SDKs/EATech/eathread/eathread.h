#ifndef EA_THREAD_EATHREAD_H
#define EA_THREAD_EATHREAD_H

#include "types.hpp"
#include <cstdint>

// The X360-faithful EA::Thread facade home (ThreadId / HANDLE, the parameter PODs,
// GetThreadId, and the EA::Thread::{Thread, Futex, IRunnable, ThreadParameters,
// RunnableFunction, RunnableFunctionUserWrapper} objects). eathread.h layers the
// SDK's base scalar types + free functions on top of it and re-exports the leaves.
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

// ===========================================================================
// SDKs/EATech/eathread/eathread.h
//
// The EAThread umbrella header (EAThread 1.09.00). It declares the SDK's base
// scalar types + constants + free functions, then pulls the sync-primitive leaves
// so a consumer can `#include "eathread/eathread.h"` and get the whole facade.
// Base-type values reconstructed from the Feb-2007 vendor eathread.h and confirmed
// against the DecFIGS DWARF (eathread.h constant table).
//
// `EA::Thread` is a vendor library boundary; its identifiers are preserved verbatim
// per the naming convention.
//
// DELEGATION / ODR FLAG: the concrete EA::Thread objects (Thread, Futex, IRunnable,
// ThreadParameters, the parameter PODs, GetThreadId) are HOMED in BrnEAThreadX360.h
// -- the asm-authoritative X360 reconstruction -- NOT here, so nothing in the leaf
// headers is redefined (a redefinition would be a C2011 collision in the many TUs
// that include BrnEAThreadX360.h AND these canonical headers, e.g.
// CgsDeviceManager.h). eathread.h only ADDS the base scalar layer
// (ThreadTime/kTimeout*/kThreadId*/priority+processor constants) and the free
// ThreadSleep(ThreadTime) overload, none of which BrnEAThreadX360.h defines.
// ===========================================================================

namespace EA
{
namespace Thread
{
    // ---- base scalar types (Feb-2007 eathread.h / DWARF) -------------------

    // 64-bit unsigned "word" type + the per-thread unique id built from it.
    typedef uint64_t Uint;
    typedef Uint     ThreadUniqueId;

    // System (OS) thread id. On this Win32/X360 build it is the same handle type as
    // ThreadId (BrnEAThreadX360.h: typedef void* ThreadId).
    typedef ThreadId SysThreadId;

    // Time is a double count of milliseconds; the two sentinel deadlines.
    typedef double ThreadTime;
    const ThreadTime kTimeoutImmediate = 0;
    const ThreadTime kTimeoutNone      = 4294967295.0;   // 0xFFFFFFFF

    // ---- id / priority / processor constants (Feb-2007 eathread.h) ---------

    const ThreadId       kThreadIdInvalid       = 0;
    const ThreadId       kThreadIdCurrent       = reinterpret_cast<ThreadId>(static_cast<uintptr_t>(0x7fffffff));
    const ThreadId       kThreadIdAny           = reinterpret_cast<ThreadId>(static_cast<uintptr_t>(0x7fffffff - 1));
    const SysThreadId    kSysThreadIdInvalid    = 0;
    const ThreadUniqueId kThreadUniqueIdInvalid = 0;

    const int kThreadPriorityUnknown   = (-0x7fffffff - 1);
    const int kThreadPriorityMin       = -128;
    const int kThreadPriorityDefault   = 0;
    const int kThreadPriorityMax       = 127;
    const int kSysThreadPriorityDefault = 1001;

    const int kProcessorDefault = -1;
    const int kProcessorAny     = -2;

    // ---- free functions ----------------------------------------------------

    // Sleep the calling thread for a relative time (Feb-2007 eathread.h). This is a
    // distinct overload from BrnEAThreadX360.h's X360 primitive
    // `u32 ThreadSleep(const u32* lpuMilliseconds)` -- they differ in parameter type
    // (const ThreadTime& vs const u32*) so both coexist; callers passing a
    // ThreadTime (e.g. CgsSystem::ThreadLayout::InteruptThreadForAssert) bind here.
    void ThreadSleep(const ThreadTime& timeRelative = kTimeoutImmediate);

    // NOTE: ThreadId GetThreadId() is already declared in BrnEAThreadX360.h (the X360
    // home @0x82B42560); it is reused via the include above, not redeclared.
}
}

// ---- the sync-primitive leaves (re-exported by the umbrella) ---------------
// Ordered so the base types above are defined before the leaves that consume them
// (eathread_mutex.h uses ThreadTime/kTimeoutNone). Each leaf carries its own guard,
// so the mutual include (leaf -> eathread.h) resolves without recursion.
#include "SDKs/EATech/eathread/eathread_atomic.h"
#include "SDKs/EATech/eathread/eathread_mutex.h"
#include "SDKs/EATech/eathread/eathread_thread.h"
#include "SDKs/EATech/eathread/eathread_futex.h"

#endif // EA_THREAD_EATHREAD_H
