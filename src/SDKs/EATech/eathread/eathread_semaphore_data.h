#ifndef EA_THREAD_EASEMAPHOREDATA_H
#define EA_THREAD_EASEMAPHOREDATA_H

#include "types.hpp"

// SDKs/EATech/eathread/eathread_semaphore_data.h
//
// EASemaphoreData -- the data half of EA::Thread::Semaphore (the wrapper holds one
// EASemaphoreData mSemaphoreData member at offset 0). Reconstructed from the X360
// .XEX (BURNOUT_X360_ARTIST.XEX). This header homes:
//   EASemaphoreData::UpdateCancelCount @ 0x82B42870
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// The EATech DWARF (references/DecFIGS/dwarfdump/SDKs/EATech/include/eathread/
// eathread_semaphore.h) describes the PS3 EASemaphoreData layout
// {sys_cond_t mCV; sys_mutex_t mMutex; AtomicInt32 mnCount; bool mbValid;}. The
// BURNOUT_X360_ARTIST.XEX links a DIFFERENT, Win32-semaphore-based variant. The
// X360 asm is authoritative here, so this header reconstructs the X360 shape
// instead of the divergent PS3 DWARF. Concrete X360 facts proven from the
// disassembly:
//   * EA::Thread::Semaphore::Init (@0x82B42988): stores the OS semaphore HANDLE at
//     +0x0 (CreateSemaphoreA), the (clamped >= 0) initial count at +0x4, and the
//     intra-process flag at +0xC.
//   * EA::Thread::Semaphore::Semaphore (@0x82B43960): zeroes +0x0/+0x4/+0x8 and sets
//     +0xC (mbIntraProcess) to 1.
//   * EASemaphoreData::UpdateCancelCount (@0x82B42870): a masked-interrupt lwarx/
//     stwcx. CAS recurrence over mnCount (+0x4) and mnCancelCount (+0x8).
// The field name mnCount matches the DWARF; mnCancelCount/mbIntraProcess are the
// X360-only fields the DWARF's PS3 variant does not carry in this position.
// The conductor should decide whether to gate this X360 variant behind a platform
// branch in the vendor tree; this file does NOT modify any committed vendor copy.
// =====================================================================

namespace EA
{
namespace Thread
{
    typedef void* SemaphoreHandle; // HANDLE == void* on the Win32/X360 host
}
}

// EASemaphoreData is a top-level (non-namespaced) EA C-style data struct, matching
// the binary's mangled name and the DWARF (`struct EASemaphoreData`, no namespace).
//
// X360 layout (asm-authoritative):
//   +0x00 mhSemaphore     (HANDLE)            -- OS semaphore (CreateSemaphoreA)
//   +0x04 mnCount         (AtomicInt32)       -- available permits; negative == waiters
//   +0x08 mnCancelCount   (AtomicInt32)       -- pending cancellations to absorb
//   +0x0C mbIntraProcess  (bool)              -- in-process (no named OS object)
struct EASemaphoreData
{
    EA::Thread::SemaphoreHandle mhSemaphore;   // +0x00
    s32                         mnCount;       // +0x04
    s32                         mnCancelCount;  // +0x08
    bool                        mbIntraProcess; // +0x0C

    // @ 0x82B42870 -- absorb iAdjustment newly-available permits, first satisfying
    // any outstanding waiters (mnCount < 0) up to zero, then parking whatever is left
    // over into mnCancelCount. Both updates are lock-free CAS loops on the X360
    // (lwarx/stwcx. under masked interrupts). Called by EA::Thread::Semaphore::Wait
    // and ::Post. No-op unless iAdjustment > 0.
    void UpdateCancelCount(int iAdjustment);
};

#endif // EA_THREAD_EASEMAPHOREDATA_H
