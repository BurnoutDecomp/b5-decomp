#pragma once

#include "types.hpp"

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

    // Capacity of the inline buffer.
    u32 GetSize() const { return N; }

    // Checked element accessor (the X360 build's Array<>::Ge).
    T&       Ge(u32 luIndex)       { return maElements[luIndex]; }
    const T& Ge(u32 luIndex) const { return maElements[luIndex]; }

private:
    T   maElements[N];
    s32 miCount;                 // live-element count; -1 == not yet Constructed/Cleared
};
