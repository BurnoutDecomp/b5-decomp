#ifndef EA_THREAD_EARWMUTEXDATA_H
#define EA_THREAD_EARWMUTEXDATA_H

#include "types.hpp"

#include "SDKs/EATech/eathread/eathread_condition.h"  // EA::Thread::Condition (the homed Win32 variant)
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"      // EA::Thread::ThreadId / ConditionParameters

// On the MSVC/Windows host the X360 EA::Thread::Mutex maps to a Win32
// CRITICAL_SECTION (the same convention as eathread_condition.h's mUnblockLock and
// device.h's mLock): EA::Thread::Mutex::Mutex(p,0,1) -> InitializeCriticalSection.
#include <windows.h>  // CRITICAL_SECTION

// ===========================================================================
// SDKs/EATech/eathread/eathread_rwmutex_data.h
//
// EARWMutexData -- the data half of EA::Thread::RWMutex (the RWMutex object holds
// one EARWMutexData mRWMutexData member). Reconstructed from the X360 .XEX
// (BURNOUT_X360_ARTIST.XEX). This header homes:
//   EARWMutexData::EARWMutexData @ 0x82B43C18  (ctor, called by RWMutex::RWMutex)
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// The BURNOUT_X360_ARTIST.XEX links the generic two-condition Win32 RWMutex
// variant, whose EARWMutexData is exactly the vendor's generic EARWMutexData
//   {int mnReadWaiters; int mnWriteWaiters; int mnReaders; ThreadId mThreadIdWriter;
//    Mutex mMutex; Condition mReadCondition; Condition mWriteCondition;}
// (b5-decomp/vendor/EAThread/include/eathread/eathread_rwmutex.h). The X360 asm is
// authoritative here, so this header reconstructs the X360 shape (with the homed
// X360 Condition and a host CRITICAL_SECTION standing in for the X360 Mutex's
// critical section) rather than #including the divergent vendor header. The sibling
// committed eathread_rwmutex.{h,cpp} reconstruct RWMutex itself by inlining these
// same members directly into RWMutex (no separate Data member); this Data struct is
// the X360-faithful split that the binary's RWMutex::RWMutex ctor actually builds.
// This file does NOT modify any committed vendor copy and does NOT redefine RWMutex.
//
// X360 byte layout proven from the disassembly (EARWMutexData::EARWMutexData
// @ 0x82B43C18):
//   +0x00 mnReadWaiters    (int)       -- zeroed (stw r30,0(r31))
//   +0x04 mnWriteWaiters   (int)       -- zeroed (stw r30,4(r31))
//   +0x08 mnReaders        (int)       -- zeroed (stw r30,8(r31))
//   +0x0C mThreadIdWriter  (ThreadId)  -- zeroed (stw r30,0xC(r31))
//   +0x10 mMutex           (CRITICAL_SECTION + lock count) -- built with
//                          EA::Thread::Mutex::Mutex(this+0x10, 0, 1) (r4=0 pParams,
//                          r5=1 bInitialize) -> InitializeCriticalSection
//   +0x40 mReadCondition   (Condition) -- EAConditionData ctor + Condition::Init
//                          (this+0x40, ConditionParameters{intra=1, name=""})
//   +0xA0 mWriteCondition  (Condition) -- EAConditionData ctor + Condition::Init
//                          (this+0xA0, ConditionParameters{intra=1, name=""})
// The host Condition is gate-compiled with its real committed size, so the +0x40 /
// +0xA0 strides document the X360 (32-bit-pointer) layout rather than constraining
// the host one -- exactly as the sibling EABarrierData/EASemaphoreData TUs.
// =====================================================================

// EARWMutexData is a top-level (non-namespaced) EA C-style data struct, matching the
// binary's mangled name and the vendor header (`struct EARWMutexData`, no namespace)
// -- the same convention as the sibling EABarrierData / EASemaphoreData.
struct EARWMutexData
{
    s32                    mnReadWaiters;    // +0x00 -- waiters wanting a read lock
    s32                    mnWriteWaiters;   // +0x04 -- waiters wanting the write lock
    s32                    mnReaders;        // +0x08 -- active read-lock count
    EA::Thread::ThreadId   mThreadIdWriter;  // +0x0C -- writer's thread id, 0 if none
    CRITICAL_SECTION       mMutex;           // +0x10 -- guards the above (X360 Mutex)
    EA::Thread::Condition  mReadCondition;   // +0x40 -- readers wait here
    EA::Thread::Condition  mWriteCondition;  // +0xA0 -- writers wait here

    // @ 0x82B43C18 -- zero the four counters (+0x00..+0x0C), construct the embedded
    // Mutex (InitializeCriticalSection of mMutex), then construct each embedded
    // Condition's data half and Init it from a ConditionParameters{intra=true,
    // name=""}. Field meanings mirror the vendor's generic EARWMutexData.
    EARWMutexData();

private:
    // Prevent default generation of copy/assign (declared, not defined) -- mirrors
    // the vendor EARWMutexData, whose embedded Mutex/Conditions are non-copyable.
    EARWMutexData(const EARWMutexData& rhs);
    EARWMutexData& operator=(const EARWMutexData& rhs);
};

#endif // EA_THREAD_EARWMUTEXDATA_H
