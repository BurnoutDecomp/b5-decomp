#pragma once

#include <cstdint>
#include "eathread/eathread_atomic.h"   // EA::Thread::AtomicInt<T> (committed vendor type)

// Futex::AtomicUint64 -- the engine's facade name for the EA threading SDK's
// lock-free 64-bit atomic. The DecFIGS DWARF resolves
// DataStreamResultReader::mEncodedStatus to BOTH `Futex::AtomicUint64`
// (CgsDataStreamResultReader.h:99) and `EA::Thread::AtomicInt<std::uint64_t>`
// (eathread_atomic.h:299) -- i.e. Futex::AtomicUint64 is a typedef of the latter.
// Use the committed vendor type rather than forking it (the full AtomicInt -- with
// GetValue / SetValue / SetValueConditional / the value ctor -- already lives at
// vendor/EAThread/include/eathread/eathread_atomic.h).
namespace Futex
{
    typedef EA::Thread::AtomicInt<std::uint64_t> AtomicUint64;
}
