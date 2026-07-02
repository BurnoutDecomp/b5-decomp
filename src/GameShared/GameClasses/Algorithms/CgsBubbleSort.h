#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>

// CgsAlgorithms::BubbleSort - the fixed-array bubble sort the director utilities
// run over their small tables (DWARF home CgsBubbleSort.h; the DecFIGS dump also
// records a 3-arg (Array&, int, int) range variant -- grown when a consumer
// lands). DECLARATION-ONLY: each <T,N> instantiation the X360 emits is its own
// ledger function (e.g. BubbleSort<AllVehicleData::NearestCarInfo,8>
// @0x822334AC's callee); bodies land with the algorithm's own TU. Elements are
// ordered through T::operator> (the DWARF declares it on the element records).
namespace CgsAlgorithms
{
    template <typename T, s32 TI_SIZE>
    void BubbleSort(Array<T, TI_SIZE>& lrArray);
}
