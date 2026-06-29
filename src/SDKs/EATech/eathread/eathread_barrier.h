#ifndef EA_THREAD_EABARRIER_H
#define EA_THREAD_EABARRIER_H

#include "types.hpp"

#include "SDKs/EATech/eathread/eathread_barrier_data.h"  // EABarrierData (the homed data half)
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"         // EA::Thread::BarrierParameters / SemaphoreParameters

// ===========================================================================
// SDKs/EATech/eathread/eathread_barrier.h
//
// EA::Thread::Barrier -- the X360 thread barrier linked into
// BURNOUT_X360_ARTIST.XEX. The Barrier object holds exactly one EABarrierData
// member (the data half already homed in eathread_barrier_data.h) at offset 0, so
// the Barrier's field offsets ARE EABarrierData's. Reconstructed from the X360 .XEX
// (per-addr exports under .ida-exports/BURNOUT_X360_ARTIST.XEX/). This header homes:
//   EA::Thread::Barrier::Barrier @ 0x82B43AD8  (ctor)
//   EA::Thread::Barrier::Init    @ 0x82B430C8  (set height + build the two sems)
//   EA::Thread::Barrier::Wait    @ 0x82B43168  (two-phase rendezvous)
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// See eathread_barrier_data.h's FLAG: the X360 binary links the generic
// "all other platforms" two-semaphore barrier variant (EABarrierData =
// {mnCurrent, mnHeight, mnIndex, Semaphore mSemaphore0, Semaphore mSemaphore1}),
// not the PS3/Posix variant the newer vendor DWARF describes. The X360 asm is
// authoritative; this header reconstructs the X360 wrapper around the homed
// EABarrierData rather than #including the divergent vendor header. The conductor
// should decide whether to gate this X360 variant behind a platform branch; this
// file does NOT modify any committed vendor copy.
//
// X360 facts proven from the disassembly:
//   * Barrier holds one EABarrierData at +0 (mnCurrent@0, mnHeight@4, mnIndex@8,
//     mSemaphore0@0xC, mSemaphore1@0x1C). The ctor builds the data half then Init.
//   * Wait returns a bool-as-int derived from the X360 `(cntlzw(...) & 0x20) == 0`
//     reduction -- 1 for the thread that observed the index flip, matching the
//     EAThread "was-this-the-releasing-thread" return.
// =====================================================================

namespace EA
{
namespace Thread
{
    class Barrier
    {
    public:
        // @ 0x82B43AD8 -- construct the EABarrierData half, then Init. When
        // pBarrierParameters is null but bInitialize is true, Init is driven from a
        // default-constructed BarrierParameters{height=0, intra=true, name=""} built
        // on the stack (the asm sets v8.mnHeight=0, v8.mbIntraProcess=1, name=0).
        Barrier(const BarrierParameters* pBarrierParameters = 0, bool bInitialize = true);

        // @ 0x82B430C8 -- if pBarrierParameters is null or the barrier is already
        // initialised (mnHeight != 0) return false; else set mnHeight and mnCurrent
        // to the requested height, then Init both phase semaphores from
        // SemaphoreParameters{count=0, intra=params->mbIntraProcess, name=0}. Returns
        // true on success.
        bool Init(const BarrierParameters* pBarrierParameters);

        // RESULT (asm-authoritative): Wait returns an int. The releasing thread (the
        // one that flips mnIndex) returns the bool reduction of the X360 cntlzw test;
        // a cancelled inner Semaphore::Wait returns kResultError(-2).
        enum Result
        {
            kResultError = -2
        };

        // @ 0x82B43168 -- rendezvous: atomically decrement mnCurrent. The non-last
        // arrivals block on the active phase semaphore; the last arrival resets
        // mnCurrent to mnHeight, releases the other arrivals (Semaphore::Post), then
        // every thread flips to the next phase by CAS-ing mnIndex. Returns the X360
        // bool reduction, or kResultError(-2) if an inner wait was cancelled.
        int Wait();

    private:
        EABarrierData mBarrierData;  // +0x00 (the only member; offset-0 data half)

        // Non-copyable (embedded Semaphores are non-copyable).
        Barrier(const Barrier& rhs);
        Barrier& operator=(const Barrier& rhs);
    };
}
}

#endif // EA_THREAD_EABARRIER_H
