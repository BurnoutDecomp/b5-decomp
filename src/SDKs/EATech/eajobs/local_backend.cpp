#include "SDKs/EATech/eajobs/local_backend.h"

#include <intrin.h> // _InterlockedCompareExchange (MSVC atomic CAS intrinsic)

// ============================================================================
// SDKs/EATech/eajobs/local_backend.cpp
//
// EA::Jobs::LocalBackend::JobInstance::AutoTryLockEventList ctor/dtor, reconstructed
// store-for-store from the X360 .XEX:
//   AutoTryLockEventList::AutoTryLockEventList @ 0x82BCA658
//   AutoTryLockEventList::~AutoTryLockEventList @ 0x82BC9C68
//
// ctor (@ 0x82BCA658):
//   *this        = pJobInstance        ; stw r4,0(r3)
//   this->locked = false               ; stb r10(=0),4(r3)
//   r11 = &pJobInstance->lock          ; addi r11,r4,0x284
//   if (*r11 != 0) return              ; lwz r9,0(r11) / bnelr  (already held -> bail)
//   // try-CAS the lock 0 -> 1:
//   reservation: old = lock; if (old == 0) lock = 1; observe `old`
//   if (old == 0) this->locked = true  ; the lock was free and we took it
//
// dtor (@ 0x82BC9C68):
//   if (!this->locked) return          ; lbz r11,4(r3) / beqlr
//   // CAS the lock 1 -> 0 (release what we took):
//   reservation: if (lock == 1) lock = 0
//
// The masked-interrupt lwarx/stwcx. reservation is modeled on the MSVC host with an
// interlocked compare-and-swap -- the same conditional atomic the PowerPC primitive
// provides.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
namespace LocalBackend
{
    // @ 0x82BCA658
    JobInstance::AutoTryLockEventList::AutoTryLockEventList(JobInstance* pJobInstance)
        : mpJobInstance(pJobInstance)
        , mbLocked(false)
    {
        volatile long* lpLock =
            reinterpret_cast<volatile long*>(&pJobInstance->mEventListLock);

        // Fast bail-out: if the lock already reads non-zero, don't even attempt the
        // reservation (the asm's `lwz r9,0(r11) / bnelr`).
        if (*lpLock != 0)
            return;

        // try-CAS 0 -> 1; mbLocked records whether we actually won it.
        long lObserved = _InterlockedCompareExchange(lpLock, 1, 0);
        if (lObserved == 0)
            mbLocked = true;
    }

    // @ 0x82BC9C68
    JobInstance::AutoTryLockEventList::~AutoTryLockEventList()
    {
        if (!mbLocked)
            return;

        volatile long* lpLock =
            reinterpret_cast<volatile long*>(&mpJobInstance->mEventListLock);

        // Release: CAS 1 -> 0 (only ever called when we hold the lock).
        _InterlockedCompareExchange(lpLock, 0, 1);
    }
}
}
}
