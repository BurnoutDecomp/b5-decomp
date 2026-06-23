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

    // Checked indexed accessor (X360 Array<T,N>::operator[]). The X360 body asserts the
    // array was Construct/Clear'd (miCount != the -1 sentinel) then bounds-checks the index
    // against the live count, streaming "Array index out of bounds. Index: <i>, length: <n>"
    // before returning &maElements[luIndex]. Generic body shared by every instantiation:
    //   non-const  CgsArray.h:538/539  (X360 0x823AFEC8 [sizeof T 24], 0x82200248 [116], 0x8241D4F8 [44])
    //   const      CgsArray.h:556/557  (X360 0x8235B918 [sizeof T 48], 0x82200140 [116], 0x8235BB50 [44])
    T& operator[](u32 luIndex)
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
        return maElements[luIndex];
    }
    const T& operator[](u32 luIndex) const
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
        return maElements[luIndex];
    }

    // GetItem - the DWARF spells the indexed accessor as Array<T,N>::GetItem (e.g.
    // GameMainFlowController dispatches its active state via maStates.GetItem(index)).
    T&       GetItem(u32 luIndex)       { return (*this)[luIndex]; }
    const T& GetItem(u32 luIndex) const { return (*this)[luIndex]; }

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

    // Drop the array back to the pre-Construct state: store the KI_UNCONSTRUCTED(-1)
    // sentinel into the count word so the next access fires the "Array used before
    // Construct/Clear was called" assert. The X360 emits this as a single `stw -1`
    // into the count member from an owner's constructor (e.g. CgsGui::AnimData::AnimData
    // @0x824E8FC8 stores -1 into both embedded arrays' count words @+0x90/+0xAC before
    // any Construct runs). Additive accessor over the existing miCount field; does not
    // change layout/sizeof.
    void MarkUnconstructed() { miCount = KI_UNCONSTRUCTED; }

    // Initialise a FIXED index->element table as fully populated: live count = capacity N. Used
    // where the X360 fills every slot by index (rather than growing via Append) -- e.g.
    // GameMainFlowController::Construct (X360 0x823C6440) stores N(7) directly into maStates' count
    // word @ +0x4C, then assigns all N entries; the checked operator[] then bounds-checks indices
    // 0..N-1 against this count. Distinct from Construct()/Clear() (empty list, count=0, Append-grown).
    void SetFullCount() { miCount = static_cast<s32>(N); }

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

    // ===== Added for the Array<LandmarkIndex,16> TU =====
    // Linear search for the first element equal to lrElement. Returns the index, or the
    // KI_UNCONSTRUCTED(-1) sentinel when absent/empty (X360 0x8231AB70 returns -1 literally;
    // DWARF spells the return uint32_t with KU_INVALID==0xffffffff -- same bit pattern, X360
    // signed -1 authoritative). Asserts the array was Construct/Clear'd.
    s32 FindFirstInstanceOf(const T& lrElement) const
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        const u32 luCount = static_cast<u32>(miCount);
        if (luCount == 0)
        {
            return KI_UNCONSTRUCTED;
        }
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            if (maElements[luIndex] == lrElement)
            {
                return static_cast<s32>(luIndex);
            }
        }
        return KI_UNCONSTRUCTED;
    }

    // True when lrElement is present (X360 0x82325220: FindFirstInstanceOf(..) != -1). The double
    // constructed-assert (Contains then FindFirstInstanceOf) is faithful to the X360.
    bool Contains(const T& lrElement) const
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        return FindFirstInstanceOf(lrElement) != KI_UNCONSTRUCTED;
    }

    // Checked element accessor (the X360 build's Array<>::Ge); same checked body as
    // operator[] (constructed-assert + bounds-check), routed through it so the bounds
    // logic lives in one place.
    T&       Ge(u32 luIndex)       { return (*this)[luIndex]; }
    const T& Ge(u32 luIndex) const { return (*this)[luIndex]; }

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

    // Reserve the next free slot and return a pointer to it WITHOUT writing a value (the caller
    // fills the returned element in place). X360 Array<T,N>::AddNew (generic body; e.g. the
    // Array<CarScoreData::ChainableMultiplierInfo,8> instantiation @ 0x823177E8). Same two
    // guards as Append; returns &maElements[miCount] then post-increments the count.
    T* AddNew()
    {
        CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
        CGS_ASSERT(static_cast<u32>(miCount) < N, "Array container out of space");
        T* lpNewElement = &maElements[miCount];
        ++miCount;
        return lpNewElement;
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
