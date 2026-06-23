#include "SDKs/EATech/eajobs/reference_count.h"

#include <intrin.h> // _InterlockedCompareExchange (MSVC atomic CAS intrinsic)

// ============================================================================
// SDKs/EATech/eajobs/reference_count.cpp
//
// EA::Jobs::Detail::ReferenceCount::Increment @ 0x82BCA520
// EA::Jobs::Detail::ReferenceCount::Decrement @ 0x82BCA580
//
// X360 control flow (Increment shown; Decrement is identical bar the guard and the
// delta):
//   loop:
//     v5 = *this                       ; lwz r11,0(r3)
//     if (*this == 0) return 0         ; cmplwi r11,0 / beq -> li r3,0
//     v8 = v5 + 1                      ; addi r8,r11,1
//     reservation CAS this: v5 -> v8   ; lwarx/stwcx. inside mfmsr/mtmsree mask
//     retry while the observed old value != v5
//   return 1
// Decrement: guard is `*this <= 1` (cmplwi r11,1 / bgt continues) and delta is -1.
//
// The masked-interrupt lwarx/stwcx. reservation pair is modeled on the MSVC host
// with an interlocked compare-and-swap retried until it observes the value it
// expected -- the same conditional-atomic-overwrite the PowerPC reservation gives.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
namespace Detail
{
    // @ 0x82BCA520
    bool ReferenceCount::Increment()
    {
        volatile long* lpCount = reinterpret_cast<volatile long*>(&mCount);
        for (;;)
        {
            long lOld = *lpCount;
            if (lOld == 0)
                return false; // dead object -- refuse to revive.
            long lObserved = _InterlockedCompareExchange(lpCount, lOld + 1, lOld);
            if (lObserved == lOld)
                return true;  // CAS held -> incremented.
            // otherwise the reservation was lost; re-read and retry.
        }
    }

    // @ 0x82BCA580
    bool ReferenceCount::Decrement()
    {
        volatile long* lpCount = reinterpret_cast<volatile long*>(&mCount);
        for (;;)
        {
            long lOld = *lpCount;
            if (lOld <= 1)
                return false; // at the floor -- never drop to/through 0 here.
            long lObserved = _InterlockedCompareExchange(lpCount, lOld - 1, lOld);
            if (lObserved == lOld)
                return true;  // CAS held -> decremented.
        }
    }
}
}
}
