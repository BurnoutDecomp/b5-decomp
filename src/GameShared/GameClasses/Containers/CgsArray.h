#pragma once

#include "types.hpp"

// Array<T, N> - a thin fixed-size array wrapper used across the Cgs containers
// (e.g. StateLoadingHelper's request dirty list). Recovered from the DecFIGS DWARF,
// which spells it as the unqualified Array<T,N>.
template <typename T, u32 N>
class Array
{
public:
    static const u32 KU_SIZE = N;

    T&       operator[](u32 luIndex)       { return maElements[luIndex]; }
    const T& operator[](u32 luIndex) const { return maElements[luIndex]; }

    // GetItem - the DWARF spells the indexed accessor as Array<T,N>::GetItem (e.g.
    // GameMainFlowController dispatches its active state via maStates.GetItem(index)).
    T&       GetItem(u32 luIndex)       { return maElements[luIndex]; }
    const T& GetItem(u32 luIndex) const { return maElements[luIndex]; }

    u32 GetSize() const { return N; }

private:
    T maElements[N];
};
