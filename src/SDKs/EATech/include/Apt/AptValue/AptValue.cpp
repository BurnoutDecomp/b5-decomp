// ===========================================================================
// EATech Apt -- AptValue out-of-line virtual bodies.
//
// SHAPE from the Feb-2007 leak (AptValue.h:509 / :535); BODIES confirmed
// against the X360 ARTIST.XEX pseudocode:
//     AptValue::DeleteThis    @ 0x824ED4B8
//     AptValue::ForceDelete   @ 0x824ED4D8
//
// X360 0x824ED4B8 (DeleteThis): a null guard then a virtual call into the
//   scalar-deleting-destructor slot (vtbl +0x38) with the delete flag set --
//   exactly the codegen of `delete this`. The leak body is `delete this`.
// X360 0x824ED4D8 (ForceDelete): vtbl +0x24 (PreDestroy), then vtbl +0x28
//   (DestroyGCPointers), then vtbl +0x38 (`delete this`). Matches the leak's
//   PreDestroy(); DestroyGCPointers(); delete this;.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"

#include <intrin.h>   // _InterlockedExchange (the Apt GC flag lock)

// The Apt GC reference-registration callback -- wired by the Apt GC startup
// (AptInit); null until then (FLAG). The GC value types' RegisterReferences call
// through it, so the mark walk is inert until the collector is up.
AptValue::ReferenceRegistrationCb AptValue::sReferenceRegistrationCb = 0;

// AptGC::CleanAll sets this while it tears the value graph down (X360 byte_8324E38E).
bool AptValue::sbSuspendRefcountDeletions = false;

// The Apt GC lock (X360 unk_8324E75C) -- a single-word spin lock guarding the GC
// mark / deferred-release flag bit mutations. FLAG: the console uses the
// lwarx/stwcx. interrupt-masked idiom; modelled as an interlocked test-and-set
// (host-portable, uncontended on the single-thread bring-up path).
namespace
{
    volatile long gAptGCFlagLock = 0;
    inline void AptGCFlagLock_Acquire() { while (_InterlockedExchange(&gAptGCFlagLock, 1) != 0) {} }
    inline void AptGCFlagLock_Release() { _InterlockedExchange(&gAptGCFlagLock, 0); }
}

// setGCMark @0x82AD8020 -- set mbHasRegisterReferenceMark under the GC lock.
void AptValue::setGCMark(bool bMark)
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mbHasRegisterReferenceMark = bMark ? 1u : 0u;
    AptGCFlagLock_Release();
}

// ClearReleaseAtEnd @0x82AD82A8 -- clear mbIsInDeferredVector under the GC lock.
void AptValue::ClearReleaseAtEnd()
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mbIsInDeferredVector = 0u;
    AptGCFlagLock_Release();
}

// incGCRoot @0x82AD8120 -- bump the GC-root count one, under the Apt GC flag
// lock. X360: acquire the lwarx/stwcx spin lock on unk_8324E75C, then
//   if ((*(this+4) & 0x3F00) < 0x3F00) <add 0x100 to the field>
// i.e. if the 6-bit mnGCRootCount is below MAX_GCROOT, increment it (setGCRoot
// clamps anyway), then release the lock.
void AptValue::incGCRoot()
{
    AptGCFlagLock_Acquire();
    if (getGCRoot() < MAX_GCROOT)
        setGCRoot(getGCRoot() + 1);
    AptGCFlagLock_Release();
}

// decGCRoot @0x82AD81A8 -- drop the GC-root count one (floored at 0), under the
// Apt GC flag lock. X360: acquire the spin lock, then
//   if (((*(this+4) >> 8) & 0x3F) != 0) <field = field - 1>
// i.e. if mnGCRootCount is non-zero, decrement it, then release the lock.
void AptValue::decGCRoot()
{
    AptGCFlagLock_Acquire();
    if (getGCRoot() > 0)
        setGCRoot(getGCRoot() - 1);
    AptGCFlagLock_Release();
}

// SetReleaseAtEnd @0x82AD8230 -- mark this value as queued in the GC deferred-
// release vector, under the Apt GC flag lock. X360: acquire the spin lock, then
//   *(this+4) |= 0x20000000   (oris r10,r10,0x2000)
// which sets the big-endian bit for mbIsInDeferredVector, then release the lock.
// (The "second locked block" in the pseudocode is the lock release itself.)
void AptValue::SetReleaseAtEnd()
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mbIsInDeferredVector = 1u;
    AptGCFlagLock_Release();
}

// DeleteThis @ 0x824ED4B8
void AptValue::DeleteThis()
{
    // X360: `if (this) <scalar deleting destructor>(this, 1)`. `delete this`
    // on a non-null receiver lowers to exactly that virtual dispatch.
    delete this;
}

// ForceDelete @ 0x824ED4D8
void AptValue::ForceDelete()
{
    PreDestroy();
    DestroyGCPointers();
    delete this;
}
