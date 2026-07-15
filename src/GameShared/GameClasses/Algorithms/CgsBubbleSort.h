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
    // 3-arg range variant (X360 ___BubbleSort_<T>_Array<T,N>(Array&, int, int), the tail
    // callee of the 1-arg entry below -- ledger-external / un-attested body: the X360 marks
    // it [external/unknown] and no asm for its inner comparison/swap loop is recovered here).
    // DECLARATION-ONLY + FLAG: sorts lrArray over the inclusive index window [liFirst, liLast]
    // through T::operator>; its body lands with whatever TU homes the worker. Declared so the
    // 1-arg entry can express its attested tail-call by name rather than fabricating the loop.
    template <typename T, s32 TI_SIZE>
    void BubbleSort(Array<T, TI_SIZE>& lrArray, s32 liFirst, s32 liLast);

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
