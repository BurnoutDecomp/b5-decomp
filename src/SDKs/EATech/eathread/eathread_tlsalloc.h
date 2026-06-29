#ifndef EA_THREAD_EATLSALLOC_H
#define EA_THREAD_EATLSALLOC_H

#include "types.hpp"

// ===========================================================================
// SDKs/EATech/eathread/eathread_tlsalloc.h
//
// EA::Thread::TLSAlloc -- the X360 EAThread helper (in BURNOUT_X360_ARTIST.XEX)
// that lazily allocates the ONE process-wide OS thread-local-storage slot the
// library uses to stash the current thread's bookkeeping pointer. Reconstructed
// from the X360 .XEX (per-addr exports under .ida-exports/BURNOUT_X360_ARTIST.XEX/):
//   EA::Thread::TLSAlloc::TLSAlloc @ 0x82B42478  (ctor)
//
// X360 fact (asm-authoritative @ 0x82B42478): the ctor takes only `this` (r3) and
// returns it unchanged. It writes NOTHING into the object -- it touches only a
// single file-scope static OS-TLS index (the binary's `dword_82F862A8`):
//
//   if (gTlsIndex == (DWORD)-1)        // cmpwi r11, -1
//       gTlsIndex = TlsAlloc();        // bl TlsAlloc; stw r3, dword_82F862A8
//   return this;                       // mr r3, r30
//
// i.e. the slot is allocated exactly once, on first construction, and the -1
// sentinel (== TLS_OUT_OF_INDEXES) marks "not yet allocated". Because the object
// carries no instance state, TLSAlloc is an empty class -- the static index is the
// only state and it is process-wide, exactly as the asm shows.
//
// `EA::Thread` is a vendor library boundary, so its identifiers (EA, Thread,
// TLSAlloc) are preserved verbatim per the naming convention.
// ===========================================================================

namespace EA
{
namespace Thread
{
    class TLSAlloc
    {
    public:
        // @ 0x82B42478 -- on first construction, allocate the single process-wide
        // OS TLS slot (TlsAlloc) and cache its index in the file-scope static; on
        // every later construction this is a no-op. The object itself stores no
        // state (the asm returns `this` without writing to it).
        TLSAlloc();
    };
}
}

#endif // EA_THREAD_EATLSALLOC_H
