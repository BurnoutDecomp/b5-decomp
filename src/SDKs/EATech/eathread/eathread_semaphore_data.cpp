#include "SDKs/EATech/eathread/eathread_semaphore_data.h"

#include <intrin.h> // _InterlockedCompareExchange (MSVC atomic CAS intrinsic)

// ============================================================================
// SDKs/EATech/eathread/eathread_semaphore_data.cpp
//
// EASemaphoreData::UpdateCancelCount @ 0x82B42870, reconstructed store-for-store
// from the X360 .XEX (BURNOUT_X360_ARTIST.XEX).
//
// The X360 body is two lock-free compare-and-swap recurrences over the two atomic
// int32 fields (mnCount @ +0x4, mnCancelCount @ +0x8). Each is a PowerPC
// lwarx/stwcx. reservation under masked interrupts (mfmsr/mtmsree); on the MSVC
// host the conditional atomic is expressed with InterlockedCompareExchange, the
// same read-compare-conditional-store primitive. This is a SCALAR atomic CAS, not a
// vectorised pipeline.
//
// Semantics: given iAdjustment > 0 new permits, first hand them to any waiting
// threads (mnCount < 0 means there are |mnCount| waiters) by raising mnCount toward
// (but not past) zero; whatever permits are left after the waiters are satisfied are
// banked into mnCancelCount.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

void EASemaphoreData::UpdateCancelCount(int iAdjustment)
{
    // asm: cmpwi cr6,r4,0 / blelr cr6 -- nothing to do for a non-positive adjustment.
    if (iAdjustment <= 0)
        return;

    volatile long* const lpCount = reinterpret_cast<volatile long*>(&mnCount);

    // ---- Phase 1: satisfy waiters (only if mnCount is currently negative) -------
    long lCount = mnCount; // v2 = *(this + 4)
    if (lCount < 0)
    {
        for (;;) // loc_82B42888 -- outer recompute-and-CAS loop
        {
            // target = (lCount + iAdjustment > 0) ? 0 : lCount + iAdjustment
            long lTarget = lCount + iAdjustment;
            if (lTarget > 0)
                lTarget = 0;

            const long lExpected = lCount; // v5 = v2

            // CAS lCount -> lTarget. _InterlockedCompareExchange returns the prior
            // value; equality with lExpected means the store happened. (The X360
            // inner loop's "value changed -> abandon, store old back" plus stwcx.
            // retry collapses to this single conditional swap with an observed old.)
            const long lOld =
                _InterlockedCompareExchange(lpCount, lTarget, lExpected); // _R7

            if (lOld == lExpected)
            {
                // CAS succeeded: we applied (lTarget - lExpected) to the count.
                // Remaining adjustment = iAdjustment + (lOld - lTarget). (asm:
                // subf r11,r10,r11 ; add r4,r11,r4)
                iAdjustment += (lOld - lTarget);
                break;
            }

            // CAS failed: mnCount changed under us (lOld is the fresh value).
            lCount = lOld; // v2 = _R7
            if (lOld >= 0)
                break;     // count is no longer negative -> nothing more to hand out
            // else: still negative and changed -> recompute target and retry.
        }
    }

    // ---- Phase 2: bank any leftover permits into mnCancelCount -------------------
    // asm: cmpwi cr6,r4,0 / blelr cr6 -- skip if nothing remains.
    if (iAdjustment <= 0)
        return;

    volatile long* const lpCancel = reinterpret_cast<volatile long*>(&mnCancelCount);
    for (;;) // loc_82B428F0 -- atomic add of iAdjustment to mnCancelCount
    {
        const long lOld = *lpCancel;                       // lwarx r10
        const long lNew = iAdjustment + lOld;              // add r9,r4,r10
        if (_InterlockedCompareExchange(lpCancel, lNew, lOld) == lOld)
            break;                                         // stwcx. succeeded
    }
}
