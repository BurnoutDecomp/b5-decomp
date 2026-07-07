#ifndef EA_THREAD_EAMUTEX_H
#define EA_THREAD_EAMUTEX_H

#include "types.hpp"

#include "SDKs/EATech/eathread/eathread.h"          // ThreadTime / kTimeoutNone (Lock default arg)
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"    // EA::Thread::MutexParameters / ThreadId (REUSED, not redefined)

// ===========================================================================
// SDKs/EATech/eathread/eathread_mutex.h
//
// EA::Thread::Mutex + AutoMutex -- the EAThread recursive mutex. Shape from the
// DecFIGS DWARF (eathread_mutex.h: EAMutexData {sys_mutex_t mMutex; int mnLockCount;
// ThreadId mThreadId;}, then Mutex {protected EAMutexData mMutexData; ...}) and the
// Feb-2007 vendor source (method set + the inline GetPlatformData / AutoMutex).
//
// `EA::Thread` is a vendor library boundary; its identifiers (Mutex, Lock, Unlock,
// AutoMutex, ...) are preserved verbatim per the naming convention.
//
// REUSE FLAG: EA::Thread::MutexParameters is HOMED in BrnEAThreadX360.h (the
// X360-authoritative {mbIntraProcess@0; char mName[16]@1} layout) and is reused via
// the include above -- it is deliberately NOT redefined here (a second definition
// would be a C2011 collision with every TU that also pulls BrnEAThreadX360.h).
//
// HOST-PRIMITIVE FLAG (sys_mutex_t): on the X360 the mutex's platform half is the
// kernel critical-section object. It is modelled below as an opaque, pointer-aligned
// byte buffer sized to a host Win32 CRITICAL_SECTION rather than pulling <windows.h>
// -- the same technique BrnEAThreadX360.h uses for the Futex critical-section
// storage, chosen so the eathread.h umbrella that re-exports this header stays free
// of <windows.h>. Every Mutex method is out-of-line (its body lives in the separate,
// not-yet-homed X360 Mutex TU), so the header never reads this buffer's internals;
// declarations suffice for the compile gate.
// ===========================================================================

// sys_mutex_t -- host stand-in for the X360 platform mutex object (see FLAG above).
// A union with a void* member forces pointer alignment; 40 bytes >= the x64 host
// sizeof(CRITICAL_SECTION).
union sys_mutex_t
{
    u8    mData[40];
    void* mAlign;
};

// EAMutexData -- the mutex's data half (global scope, matching the DWARF and
// Feb-2007 vendor source). Member order is DWARF-authoritative.
struct EAMutexData
{
    sys_mutex_t          mMutex;        // +0x00  platform critical-section object
    int                  mnLockCount;   //        recursive acquire depth
    EA::Thread::ThreadId mThreadId;     //        current owner (0 when unlocked)

    // Out-of-line in the X360 Mutex TU (zeroes the fields + initialises mMutex).
    EAMutexData();
};

namespace EA
{
namespace Thread
{
    class Mutex
    {
    public:
        // Mutex::Result sentinels (Feb-2007 eathread_mutex.h:168). Lock returns the
        // post-acquire recursion count on success, or one of these on failure.
        enum Result
        {
            kResultError   = -1,
            kResultTimeout = -2
        };

        // Construct (and, unless bDefaultParameters suppresses it, Init) the mutex.
        Mutex(const MutexParameters* pMutexParameters = 0, bool bDefaultParameters = true);

        // Destroy the underlying OS object.
        ~Mutex();

        // Build the OS object from pMutexParameters; true on success.
        bool Init(const MutexParameters* pMutexParameters);

        // Acquire the lock (recursive), blocking until timeoutAbsolute. Returns the
        // new recursion count, kResultTimeout, or kResultError.
        int Lock(const ThreadTime& timeoutAbsolute = kTimeoutNone);

        // Release one recursion level. Returns the remaining recursion count.
        int Unlock();

        // Current recursion depth held by the calling context.
        int GetLockCount() const;

        // True if the calling thread currently owns the lock.
        bool HasLock() const;

        // Inline in the Feb-2007 vendor source: hands back the embedded data half.
        void* GetPlatformData() { return &mMutexData; }

    protected:
        EAMutexData mMutexData;   // DWARF eathread_mutex.h:259 (protected, offset 0)

    private:
        // Non-copyable (declared, not defined -- Feb-2007 makes these private).
        Mutex(const Mutex&);
        Mutex& operator=(const Mutex&);
    };

    // AutoMutex -- scoped lock guard. DWARF eathread_mutex.h:316 pins the member as
    // `const Mutex& mMutex`; Lock/Unlock are non-const in this build, so the inline
    // ctor/dtor cast the const away to drive them (the emitted acquire/release is
    // identical either way). Non-copyable per the DWARF.
    class AutoMutex
    {
    public:
        AutoMutex(Mutex& mutex)
            : mMutex(mutex)
            { const_cast<Mutex&>(mMutex).Lock(); }

        ~AutoMutex()
            { const_cast<Mutex&>(mMutex).Unlock(); }

    protected:
        const Mutex& mMutex;

    private:
        AutoMutex(const AutoMutex&);
        const AutoMutex& operator=(const AutoMutex&);
    };
}
}

#endif // EA_THREAD_EAMUTEX_H
