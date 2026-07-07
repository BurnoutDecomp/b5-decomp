#ifndef EA_THREAD_EAFUTEX_H
#define EA_THREAD_EAFUTEX_H

#include "types.hpp"

// EA::Thread::Futex is HOMED in BrnEAThreadX360.h -- the CRITICAL_SECTION-backed
// fast lock the X360/Win32 build links (EATHREAD_MANUAL_FUTEX_ENABLED is OFF, so
// the ctor RtlInitializeCriticalSection's an embedded critical section and
// Lock/Unlock/TryLock are the Enter/Leave/TryEnter calls). Reuse it; do not redefine.
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

// ===========================================================================
// SDKs/EATech/eathread/eathread_futex.h
//
// The EAThread futex header. It re-exports the project's EA::Thread::Futex (homed
// in BrnEAThreadX360.h) and adds the scoped-guard EA::Thread::AutoFutex.
//
// `EA::Thread` is a vendor library boundary; its identifiers are preserved verbatim.
//
// DELEGATION / ODR FLAG: EA::Thread::Futex is NOT redefined here. The DecFIGS DWARF
// eathread_futex.h describes the ALTERNATE "manual" futex layout ({AtomicUint64
// mUseCount; Uint mRecursionCount; ThreadUniqueId mThreadUniqueId; EAFutexSemaphore
// mSemaphore}), but the shipped X360 binary links the CRITICAL_SECTION variant, and
// every consumer of this header expects exactly that (their comments say
// "EA::Thread::Futex (CRITICAL_SECTION-backed lock)" -- CgsMoviePlayer.h,
// BrnReplayModule.h, CgsFile.h, CgsFileSystem.h, CgsStreamDeviceDiskRead.h,
// CgsDeviceManager.h). Redefining Futex here (either variant) would also C2011-collide
// with BrnEAThreadX360.h in the TUs that include both. So the class stays homed in
// BrnEAThreadX360.h and this header only supplies the guard it lacks.
//
// (Distinct from the `namespace Futex` facade in eathread/Futex.h, which is a
// SEPARATE namespace re-exporting AtomicInt<uint64_t> as Futex::AtomicUint64 -- the
// two coexist: `::Futex` (a namespace) vs `EA::Thread::Futex` (this class).)
// ===========================================================================

namespace EA
{
namespace Thread
{
    // AutoFutex -- scoped lock guard over an EA::Thread::Futex. DWARF
    // eathread_futex.h:235 pins the member as `const Futex& mFutex`; Futex::Lock /
    // Futex::Unlock are non-const, so the inline ctor/dtor cast the const away to
    // drive them (identical emitted acquire/release). Non-copyable per the DWARF.
    class AutoFutex
    {
    public:
        AutoFutex(Futex& futex)
            : mFutex(futex)
            { const_cast<Futex&>(mFutex).Lock(); }

        ~AutoFutex()
            { const_cast<Futex&>(mFutex).Unlock(); }

    protected:
        const Futex& mFutex;

    private:
        AutoFutex(const AutoFutex&);
        const AutoFutex& operator=(const AutoFutex&);
    };
}
}

#endif // EA_THREAD_EAFUTEX_H
