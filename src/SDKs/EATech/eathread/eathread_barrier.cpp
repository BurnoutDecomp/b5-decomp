// X360-faithful EA::Thread::Barrier (the generic two-semaphore Win32 barrier variant
// linked into BURNOUT_X360_ARTIST.XEX). Source of truth: per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides the divergent PS3/Posix
// DWARF). See eathread_barrier.h / eathread_barrier_data.h for the committed-type /
// vendor-snapshot divergence FLAG.
//
// Vendor EA code reconstructed in its canonical home.

#include "SDKs/EATech/eathread/eathread_barrier.h"

#include <intrin.h>  // _InterlockedDecrement / _InterlockedExchange / _InterlockedCompareExchange

namespace EA
{
namespace Thread
{

namespace
{
    // The X360 phase-semaphore Wait in Barrier::Wait blocks forever (no deadline set
    // up in r4 at the call site). The homed Semaphore::Wait dereferences a u32*; the
    // block-forever sentinel is 0xFFFFFFFF (INFINITE), so we hand it &KU_TIMEOUT_NONE.
    const u32 KU_TIMEOUT_NONE = 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// Barrier::Barrier @ 0x82B43AD8
//
//   bl EABarrierData::EABarrierData                    ; (the mBarrierData member)
//   if (a2 == 0 && (a3 & 0xFF) != 0)                   ; no params but bInitialize:
//     BarrierParameters{mnHeight=0, mbIntraProcess=1, mName=""} on the stack
//   bl EA::Thread::Barrier::Init(this, params)
// ---------------------------------------------------------------------------
Barrier::Barrier(const BarrierParameters* pBarrierParameters, bool bInitialize)
{
    // mBarrierData is constructed by its EABarrierData() ctor (member init), which
    // zeroes mnCurrent/mnHeight/mnIndex and default-builds the two Semaphores.

    BarrierParameters lDefault;  // {mnHeight, mbIntraProcess, mName} -- fill below
    if (pBarrierParameters == 0 && bInitialize)
    {
        lDefault.mnHeight       = 0;     // stw 0
        lDefault.mbIntraProcess = true;  // stb 1
        lDefault.mName[0]       = '\0';  // stb 0
        pBarrierParameters      = &lDefault;
    }

    Init(pBarrierParameters);
}

// ---------------------------------------------------------------------------
// Barrier::Init @ 0x82B430C8
//
//   if (a2 == 0)             return 0;                  ; no params
//   if (mnHeight != 0)       return 0;                  ; lwz 4(this); already init'd
//   mnHeight  = a2->mnHeight;                            ; stw 4(this)
//   mnCurrent = a2->mnHeight;                            ; atomic stwcx. this+0
//   SemaphoreParameters sp{0, a2->mbIntraProcess, 0};    ; count 0, name 0
//   Semaphore::Init(this+0xC, &sp);  // mSemaphore0
//   Semaphore::Init(this+0x1C, &sp); // mSemaphore1
//   return 1;
//
// mnCurrent counts arrivals down from mnHeight. The two phase semaphores both start
// with initial count 0 (sp.mInitialCount = 0; the asm stores 0 into the count slot).
// ---------------------------------------------------------------------------
bool Barrier::Init(const BarrierParameters* pBarrierParameters)
{
    if (pBarrierParameters == 0)
        return false;
    if (mBarrierData.mnHeight != 0)  // lwz 4(this) -- already initialised
        return false;

    mBarrierData.mnHeight  = pBarrierParameters->mnHeight;  // stw r10, 4(this)
    // mnCurrent = mnHeight via the X360 masked-interrupt lwarx/stwcx. (atomic store).
    mBarrierData.mnCurrent = pBarrierParameters->mnHeight;

    SemaphoreParameters lParams(0, pBarrierParameters->mbIntraProcess, 0);  // count 0, name ""
    mBarrierData.mSemaphore0.Init(&lParams);  // this+0xC
    mBarrierData.mSemaphore1.Init(&lParams);  // this+0x1C
    return true;
}

// ---------------------------------------------------------------------------
// Barrier::Wait @ 0x82B43168
//
//   phase = mnIndex;                                    ; v2 = *(this+8)
//   if (--mnCurrent != 0) {                              ; atomic dec this+0; not last
//       r = (phase ? mSemaphore1 : mSemaphore0).Wait(kTimeoutNone);
//       if (r == kResultError)        return kResultError; // -2
//   } else {                                             ; last arrival
//       mnCurrent = mnHeight;                            ; atomic reset this+0
//       if (mnHeight <= 1) r = 0;
//       else               r = (phase ? mSemaphore1 : mSemaphore0).Post(mnHeight - 1);
//       if (r < 0)         return -1;
//   }
//   // every thread flips the phase index from `phase` to `1 - phase` via CAS.
//   loop {
//       cur = mnIndex;                                   ; lwarx this+8
//       if (cur != phase) break;                          ; someone already flipped
//       if (CAS(mnIndex, phase -> 1-phase) succeeded) break;
//   }
//   return (cntlzw(phase - cur) & 0x20) == 0;             ; 0 if cur==phase, else 1
// ---------------------------------------------------------------------------
int Barrier::Wait()
{
    volatile long* const lpCurrent =
        reinterpret_cast<volatile long*>(&mBarrierData.mnCurrent);  // this+0
    volatile long* const lpIndex =
        reinterpret_cast<volatile long*>(&mBarrierData.mnIndex);    // this+8

    const s32 iPhase = mBarrierData.mnIndex;  // v2 = *(this+8)

    int iResult;

    // Atomically register this arrival (mnCurrent -= 1); r11 = post-decrement value.
    if (_InterlockedDecrement(lpCurrent) != 0)
    {
        // Not the last arrival: block on the active phase semaphore.
        Semaphore& rActive = iPhase ? mBarrierData.mSemaphore1 : mBarrierData.mSemaphore0;
        iResult = rActive.Wait(&KU_TIMEOUT_NONE);
        if (iResult == kResultError)  // -2
            return kResultError;
    }
    else
    {
        // Last arrival: re-arm the counter and release everyone else.
        _InterlockedExchange(lpCurrent, mBarrierData.mnHeight);  // mnCurrent = mnHeight

        const s32 iHeight = mBarrierData.mnHeight;
        if (iHeight <= 1)
        {
            iResult = 0;
        }
        else
        {
            Semaphore& rActive = iPhase ? mBarrierData.mSemaphore1 : mBarrierData.mSemaphore0;
            iResult = rActive.Post(iHeight - 1);
        }
        if (iResult < 0)
            return -1;
    }

    // Flip the phase index from iPhase to (1 - iPhase). The X360 lwarx/stwcx. loop
    // CAS-es mnIndex from iPhase to 1-iPhase, abandoning the store (and retrying the
    // reservation) if another thread already moved the index. InterlockedCompareExchange
    // performs exactly that atomic conditional swap and returns the prior value.
    const s32 iObserved = static_cast<s32>(
        _InterlockedCompareExchange(lpIndex, 1 - iPhase, iPhase));

    // (cntlzw(iPhase - iObserved) & 0x20) == 0  ->  iPhase == iObserved ? 0 : 1
    // (the releasing thread that still saw the old phase returns 0; a thread that
    // observed the index already flipped returns 1).
    return (iPhase - iObserved) != 0 ? 1 : 0;
}

} // namespace Thread
} // namespace EA
