#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Append/Erase/EraseFast bounds)

#include <cstdlib>   // std::qsort (Array<T,N>::QSort)

// Array<T, N> - a thin fixed-size array wrapper used across the Cgs containers
// (e.g. StateLoadingHelper's request dirty list). Recovered from the DecFIGS DWARF,
// which spells it as the unqualified Array<T,N>.
//
// X360 layout note (recovered from CgsArray.h assert sites in the X360 build, e.g.
// BrnGameState::StartLocation,8>::Ge @ 0x822AE208 and
// BrnGameState::GameModeParams::GetCheckpointCount @ 0x822B1EF0): the inline element
// buffer is followed by a single live-element count word, initialised to the sentinel
// -1 ("Array used before Construct/Clear was called") and set to a real length by
// Construct/Clear. Element access (Ge) bounds-checks the index against this count.
// The count member is appended *after* the element buffer to match that on-disk shape.
// Existing thin users that only declare an Array<> member (BrnGuiCache) are unaffected:
// this only adds a trailing field and accessors, it does not change how they use it.
template <typename T, u32 N>
class Array
{
public:
    static const u32 KU_SIZE          = N;
    static const s32 KI_UNCONSTRUCTED = -1; // sentinel before Construct/Clear has run

    T&       operator[](u32 luIndex)       { return maElements[luIndex]; }
    const T& operator[](u32 luIndex) const { return maElements[luIndex]; }

    // GetItem - the DWARF spells the indexed accessor as Array<T,N>::GetItem (e.g.
    // GameMainFlowController dispatches its active state via maStates.GetItem(index)).
    T&       GetItem(u32 luIndex)       { return maElements[luIndex]; }
    const T& GetItem(u32 luIndex) const { return maElements[luIndex]; }

    // Bring the array to its empty-but-usable state: zero live elements. This is what the
    // X360 build does to each embedded Array<> in its owner's Construct (a single `stw 0`
    // into the count word, e.g. GameModeParams::Construct @ 0x8231C370 stores 0 to
    // maStartLocations.miCount @ +0x250 and maCheckpointDataArray.miCount @ +0x520). It
    // flips the count off the KI_UNCONSTRUCTED(-1) sentinel so the count getters / Ge()
    // bounds checks no longer fire the "Array used before Construct/Clear was called" assert.
    // Construct() and Clear() are the two names the assert string itself attests; both have
    // the same observable effect here (the inline element buffer needs no per-element work).
    void Construct() { miCount = 0; }
    void Clear()     { miCount = 0; }

    // Number of live elements (-1 until Construct/Clear initialises it).
    s32 GetCount() const { return miCount; }

    bool IsFull() const
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        return static_cast<u32>(miCount) == N;
    }

    // Checked live-element count (the X360 build's Array<T,N>::GetLength; the DWARF spells it
    // GetLength returning u32). Asserts the array was Construct/Clear'd (count != the -1 sentinel)
    // then returns it; the unsigned return matches the DWARF. (X360 0x823AC200 = the DriveThruInfo,46
    // instantiation of this body.)
    u32 GetLength() const
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        return static_cast<u32>(miCount);
    }

    // Capacity of the inline buffer.
    u32 GetSize() const { return N; }

    // Checked element accessor (the X360 build's Array<>::Ge).
    T&       Ge(u32 luIndex)       { return maElements[luIndex]; }
    const T& Ge(u32 luIndex) const { return maElements[luIndex]; }

    // Push one element onto the inline buffer (X360 Array<T,N>::Append, generic template body
    // shared by every instantiation). Asserts the array was Construct/Clear'd and has room; the
    // X360 streamed the dynamic "Array container out of space, Length/Capacity" message, kept
    // here as a static CGS_ASSERT string. Unsigned count compare keeps the -1 sentinel failing.
    void Append(const T& lrElement)
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        CGS_ASSERT(static_cast<u32>(miCount) < N, "Array container out of space");
        maElements[miCount] = lrElement;
        ++miCount;
    }

    // Remove the element at luIndex, shifting the tail down one slot (order-preserving).
    void Erase(u32 luIndex)
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Trying to erase an unused element");
        --miCount;
        for (u32 luShift = luIndex; luShift < static_cast<u32>(miCount); ++luShift)
        {
            maElements[luShift] = maElements[luShift + 1];
        }
    }

    // Unordered erase: overwrite luIndex with the current last live element, then drop the count
    // (self-copy when luIndex is already last is intentional/harmless, matching the X360).
    void EraseFast(u32 luIndex)
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Trying to erase an unused element");
        maElements[luIndex] = maElements[miCount - 1];
        --miCount;
    }

    // In-place sort of the live elements via a caller-supplied C-style comparator (X360 used the
    // C library qsort). Returns the buffer base (the X360 void return is the this-register artifact).
    T* QSort(int (*lpfComparator)(const void* lpA, const void* lpB))
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        const u32 luCount = static_cast<u32>(miCount);
        if (luCount > 1)
        {
            std::qsort(maElements, luCount, sizeof(T), lpfComparator);
        }
        return maElements;
    }

private:
    T   maElements[N];
    s32 miCount;                 // live-element count; -1 == not yet Constructed/Cleared
};
