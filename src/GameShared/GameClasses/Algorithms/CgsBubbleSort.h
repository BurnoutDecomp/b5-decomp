#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT (constructed / non-empty invariants)

// CgsAlgorithms::BubbleSort - the fixed-array bubble sort the director utilities
// run over their small tables (DWARF home CgsBubbleSort.h; the DecFIGS dump also
// records a 3-arg (Array&, int, int) range variant). Elements are ordered through
// T::operator> (the DWARF declares it on the element records).
//
// The 1-arg entry BubbleSort<T,N>(Array<T,N>&) is bodied from the X360 asm of the
// BubbleSort<AllVehicleData::NearestCarInfo,8> instance @ 0x8222D8F8 (its own ledger
// function under class:CgsAlgorithms; called by AllVehicleData's nearest-car queries):
// it asserts the array was Construct/Clear'd (miCount != -1) and is non-empty
// (length-1 >= 0), then delegates the actual ordering to the range worker over the
// whole live window [0, length-1].
namespace CgsAlgorithms
{
    // ------------------------------------------------------------------------
    // 3-arg range variant (X360 ___BubbleSort_<T>_Array<T,N>(Array&, int, int) -- the tail
    // callee of the 1-arg entry below).
    //
    // ⭐ BODIED 2026-08-01, and the FLAG that used to sit here was WRONG. It read
    // "ledger-external / un-attested body: the X360 marks it [external/unknown] and no asm for
    // its inner comparison/swap loop is recovered here". The instantiation is fully present in
    // the export set at @0x82213F80 (it is carried under class:<global>, which is why a search
    // for it under CgsAlgorithms came back empty -- the same "a missing body is usually a NAME
    // search failing" trap this project keeps hitting). Read straight off it:
    //
    //     for (liLast = liEnd; liLast != liStart; --liLast)
    //     {
    //         liSwaps = 0;
    //         for (li = liStart; li != liLast; ++li)
    //             if (lrArray[li] > lrArray[li + 1]) { swap(li, li + 1); ++liSwaps; }
    //         if (liSwaps == 0) break;                     // the no-swap early-out
    //     }
    //
    // The comparison is T::operator> (for AllVehicleData::NearestCarInfo that compares +0x04,
    // mfDistance -- so the order is ASCENDING by distance, which is what the nearest-car
    // queries expect and what confirms NearestCarInfo::operator>'s own body). The element move
    // is a plain 3-word record copy in the instantiated asm; written here as T assignment so
    // it stays correct for any element type. The console's only assert is the range guard.
    // ------------------------------------------------------------------------
    template <typename T, s32 TI_SIZE>
    void BubbleSort(Array<T, TI_SIZE>& lrArray, s32 liFirst, s32 liLast)
    {
        CGS_ASSERT(liFirst <= liLast, "liStart <= liEnd");   // CgsBubbleSort.h:93

        for (s32 liEnd = liLast; liEnd != liFirst; --liEnd)
        {
            s32 liSwaps = 0;

            for (s32 li = liFirst; li != liEnd; ++li)
            {
                if (lrArray[li] > lrArray[li + 1])
                {
                    const T lTemp     = lrArray[li];
                    lrArray[li]       = lrArray[li + 1];
                    lrArray[li + 1]   = lTemp;
                    ++liSwaps;
                }
            }

            // Already ordered: the console breaks out rather than running the remaining passes.
            if (liSwaps == 0)
            {
                break;
            }
        }
    }

    // 1-arg entry (X360 0x8222D8F8, BubbleSort<NearestCarInfo,8>). Assert constructed +
    // non-empty, then sort the whole live window [0, GetLength()-1] via the range worker.
    template <typename T, s32 TI_SIZE>
    void BubbleSort(Array<T, TI_SIZE>& lrArray)
    {
        // *(a1+96) == -1 guard: GetLength() itself asserts miCount != KI_UNCONSTRUCTED(-1)
        // ("Array used before Construct/Clear was called").
        const s32 liLast = static_cast<s32>(lrArray.GetLength()) - 1;   // v2 = length - 1
        CGS_ASSERT(liLast >= 0, "Array is too large to sort, or it is zero size!");  // v2 < 0 guard

        BubbleSort<T, TI_SIZE>(lrArray, 0, liLast);   // ___BubbleSort_...(a1, 0, v2)
    }
}
