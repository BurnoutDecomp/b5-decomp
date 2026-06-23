#ifndef EA_THREAD_EASEMAPHORE_H
#define EA_THREAD_EASEMAPHORE_H

#include "types.hpp"

#include "SDKs/EATech/eathread/eathread_semaphore_data.h"  // EASemaphoreData (data half)
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"          // EA::Thread::SemaphoreParameters

// ===========================================================================
// SDKs/EATech/eathread/eathread_semaphore.h
//
// EA::Thread::Semaphore -- the thin Win32-semaphore wrapper class in
// BURNOUT_X360_ARTIST.XEX. The wrapper holds exactly one EASemaphoreData member
// (the data half already homed in eathread_semaphore_data.h) at offset 0, so the
// wrapper's sizeof and field offsets are EASemaphoreData's: the OS HANDLE at +0,
// mnCount at +4, mnCancelCount at +8, mbIntraProcess at +0xC. Every wrapper
// method forwards into that member's fields by name. This header homes:
//
//     EA::Thread::Semaphore::Semaphore  @ 0x82B43960  (ctor)
//     EA::Thread::Semaphore::~Semaphore @ 0x82B42970  (dtor: CloseHandle)
//     EA::Thread::Semaphore::Init       @ 0x82B42988  (BOOL; create the OS semaphore)
//     EA::Thread::Semaphore::Wait       @ 0x82B42A40  (int; acquire one permit)
//     EA::Thread::Semaphore::GetCount   @ 0x82B42CB0  (int; clamped available count)
//
// There is no Feb-2007 leak source for this TU; the SHAPE and bodies come from
// the X360 asm (per-addr exports under .ida-exports/BURNOUT_X360_ARTIST.XEX/).
// `EA::Thread` is a vendor library boundary, so its identifiers (EA, Thread,
// Semaphore, Init, Wait, Post, GetCount) are preserved verbatim per the naming
// convention.
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// b5-decomp/vendor/EAThread is a NEWER upstream EAThread snapshot whose
// eathread_semaphore.h Semaphore class wraps a different (PS3-style) data half.
// The X360 binary links a Win32-semaphore variant whose data half is the
// asm-authoritative EASemaphoreData homed in eathread_semaphore_data.h. The X360
// asm is authoritative here, so this header reconstructs the X360 wrapper instead
// of #including the divergent vendor header. The conductor should decide whether
// to gate this X360 variant behind a platform branch in the vendor tree; this
// file does NOT modify any committed vendor copy.
// =====================================================================
//
// WAIT-TIMEOUT FLAG (asm-authoritative, surprising): the X360 Wait converts its
// absolute-deadline argument to a relative Win32 timeout with a HARDCODED 10000ms
// (10s) reference window: it computes (deadline - (GetTickCount() + 10000)) and
// clamps the result at 0, leaving -1 (INFINITE) and 0 (poll) to pass through
// verbatim. This is reproduced store-for-store below; it is the binary's behaviour,
// not a paraphrase.

namespace EA
{
namespace Thread
{

// Wait() returns these sentinels alongside the post-acquire count (the X360
// leaves them in r3): kResultError (-2) when the underlying OS wait fails or the
// semaphore is cancelled; kResultTimeout (-1) when the wait times out. A
// non-negative return is the count remaining after acquiring one permit.
enum SemaphoreResult
{
    kResultError   = -2,  // OS wait failed / cancelled (UpdateCancelCount path)
    kResultTimeout = -1   // WaitForSingleObject returned WAIT_TIMEOUT (258)
};

// ---------------------------------------------------------------------------
// EA::Thread::Semaphore -- wraps a single EASemaphoreData at offset 0.
// ---------------------------------------------------------------------------
class Semaphore
{
public:
    // @ 0x82B43960 -- zero the data half (HANDLE=0, mnCount=0, mnCancelCount=0)
    // and set mbIntraProcess=1, then call Init. When pParameters is null but
    // bInitialize is true, Init is driven from a default-constructed
    // SemaphoreParameters{count=0, intraProcess=1, name=""} built on the stack.
    Semaphore(const SemaphoreParameters* pParameters = 0, bool bInitialize = true);

    // @ 0x82B42970 -- if the OS HANDLE is non-null, CloseHandle it.
    ~Semaphore();

    // @ 0x82B42988 -- create the underlying Win32 semaphore from pParameters
    // (CreateSemaphoreA with the clamped initial count and a 0x3FFFFFFF max).
    // No-op (returns false) if pParameters is null or a semaphore already exists.
    // Returns true when the OS handle was created.
    bool Init(const SemaphoreParameters* pParameters);

    // @ 0x82B42A40 -- acquire one permit. *pTimeoutAbsolute is the absolute
    // deadline (-1 == INFINITE, 0 == poll); see the WAIT-TIMEOUT FLAG above for the
    // 10s relative-window conversion. For an intra-process semaphore the count is
    // adjusted with the interrupt-masked lwarx/stwcx. atomic and the OS object is
    // only waited on when the count went negative; otherwise it waits on the OS
    // object directly. Returns the remaining count, kResultTimeout, or kResultError.
    int Wait(const u32* pTimeoutAbsolute);

    // @ 0x82B42CB0 -- return mnCount clamped at zero (a negative count means there
    // are waiters, which GetCount reports as 0 available permits).
    int GetCount() const;

private:
    EASemaphoreData mSemaphoreData;  // +0x00 (the only member; offset-0 data half)
};

} // namespace Thread
} // namespace EA

#endif // EA_THREAD_EASEMAPHORE_H
