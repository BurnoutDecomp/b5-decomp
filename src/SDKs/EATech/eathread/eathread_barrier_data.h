#ifndef EA_THREAD_EABARRIERDATA_H
#define EA_THREAD_EABARRIERDATA_H

#include "types.hpp"

#include "SDKs/EATech/eathread/eathread_semaphore.h"  // EA::Thread::Semaphore (the homed Win32 variant)

// ===========================================================================
// SDKs/EATech/eathread/eathread_barrier_data.h
//
// EABarrierData -- the data half of EA::Thread::Barrier (the Barrier object holds
// one EABarrierData mBarrierData member). Reconstructed from the X360 .XEX
// (BURNOUT_X360_ARTIST.XEX). This header homes:
//   EABarrierData::EABarrierData @ 0x82B43A50  (ctor)
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// The EATech DWARF (references/DecFIGS/dwarfdump/SDKs/EATech/include/eathread/
// eathread_barrier.h) describes the PS3/Posix EABarrierData layout
// {sys_cond_t mCV; sys_mutex_t mMutex; int mnHeight; int mnCurrent;
//  unsigned int mnCycle; bool mbValid;}. The BURNOUT_X360_ARTIST.XEX links the
// "all other platforms" semaphore-pair variant instead. The X360 asm is
// authoritative here, so this header reconstructs the X360 shape -- which matches
// the vendor's generic (non-Sony, non-Posix) EABarrierData exactly:
//   {AtomicInt32 mnCurrent; int mnHeight; AtomicInt32 mnIndex;
//    Semaphore mSemaphore0; Semaphore mSemaphore1;}
// (see b5-decomp/vendor/EAThread/include/eathread/eathread_barrier.h, the
// "#else // All other platforms" branch).
//
// Concrete X360 facts proven from the disassembly (EABarrierData::EABarrierData
// @ 0x82B43A50):
//   * +0x00 mnCurrent  : AtomicInt32, zeroed via a masked-interrupt lwarx/stwcx.
//                        (mfmsr/mtmsree) reservation store.
//   * +0x04 mnHeight   : plain int, zeroed via a plain `stw r11,4(r31)`.
//   * +0x08 mnIndex    : AtomicInt32, zeroed via the same masked-interrupt
//                        lwarx/stwcx. idiom.
//   * +0x0C mSemaphore0: EA::Thread::Semaphore (X360 16B), placement-constructed
//                        with Semaphore(0, 0) (bl Semaphore::Semaphore, r3=this+0xC).
//   * +0x1C mSemaphore1: EA::Thread::Semaphore (X360 16B), placement-constructed
//                        with Semaphore(0, 0) (bl Semaphore::Semaphore, r3=this+0x1C).
// Unlike EAConditionData this carries NO Mutex member -- the X360 ctor builds only
// the two embedded Semaphores. The two AtomicInt32 fields are modelled as plain
// s32 (the project's committed convention -- AtomicInt32 has no by-name host type;
// see the mnRefCount/busy-flag fields in BrnEAThreadX360.*). The +0xC/+0x1C strides
// document the X360 (32-bit-pointer) layout; the host Semaphore is gate-compiled
// with its real committed size, exactly as the sibling EASemaphoreData/Thread TUs.
// The conductor should decide whether to gate this X360 variant behind a platform
// branch in the vendor tree; this file does NOT modify any committed vendor copy.
// =====================================================================

// EABarrierData is a top-level (non-namespaced) EA C-style data struct, matching
// the binary's mangled name and the DWARF (`struct EABarrierData`, no namespace) --
// the same convention as the sibling EASemaphoreData.
//
// X360 layout (asm-authoritative):
//   +0x00 mnCurrent    (AtomicInt32 -> s32) -- threads still to arrive (counts down)
//   +0x04 mnHeight     (int)                -- number of threads required
//   +0x08 mnIndex      (AtomicInt32 -> s32) -- which of the two semaphores is active
//   +0x0C mSemaphore0  (EA::Thread::Semaphore) -- first phase semaphore
//   +0x1C mSemaphore1  (EA::Thread::Semaphore) -- second phase semaphore
struct EABarrierData
{
    s32                   mnCurrent;    // +0x00 (AtomicInt32)
    s32                   mnHeight;     // +0x04
    s32                   mnIndex;      // +0x08 (AtomicInt32)
    EA::Thread::Semaphore mSemaphore0;  // +0x0C
    EA::Thread::Semaphore mSemaphore1;  // +0x1C

    // @ 0x82B43A50 -- zero mnCurrent (+0x00) and mnIndex (+0x08) via the X360
    // masked-interrupt lwarx/stwcx. atomic-init idiom, zero mnHeight (+0x04) with a
    // plain store, then construct both embedded Semaphores with Semaphore(0, false)
    // (no parameters, bInitialize = false -- Barrier::Init creates the OS objects
    // later). Field meanings mirror the vendor's "all other platforms" EABarrierData.
    EABarrierData();

private:
    // Prevent default generation of copy/assign (declared, not defined) -- mirrors
    // the vendor EABarrierData, whose embedded Semaphores are non-copyable.
    EABarrierData(const EABarrierData& rhs);
    EABarrierData& operator=(const EABarrierData& rhs);
};

#endif // EA_THREAD_EABARRIERDATA_H
