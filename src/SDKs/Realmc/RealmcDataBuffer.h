#pragma once

// ===========================================================================
// RealmcIface::DataBuffer -- a Realmc save/load "data buffer" descriptor used by
// the X360 memory-card / save-load interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX; sibling to RealmcIface::CardData in RealmcCardData.h
// and the RealmcCore primitives in RealmcCore.h).
//
// This header is the canonical OWNING home for the DataBuffer struct and its one
// reconstructed member function:
//
//     RealmcIface::DataBuffer::operator=  @ 0x82B51928   (copy assign)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, DataBuffer) are preserved
// verbatim per the naming convention.
//
// LAYOUT (from asm -- operator= copies exactly two adjacent 32-bit words):
//
//   +0x00  mpData  -- pointer to the buffer's bytes
//   +0x04  muSize  -- the buffer length (bytes)
//
// operator= is a plain two-word memberwise copy (lwz/stw of word 0 then word 1),
// so DataBuffer is an 8-byte {pointer, size} descriptor. The two words are
// modelled as named members rather than a raw blob because their roles (a data
// pointer and a length) are unambiguous from how the descriptor is used.
// ===========================================================================

#include <cstddef>
#include <cstdint>

namespace RealmcIface
{

class DataBuffer
{
public:
    DataBuffer() : mpData(nullptr), muSize(0) {}

    // @ 0x82B51928 -- copy the source's two words (mpData then muSize) over ours.
    //                 lwz r11,0(r4); stw r11,0(r3); lwz r11,4(r4); stw r11,4(r3).
    DataBuffer& operator=(const DataBuffer& rOther);

    void*         mpData;  // +0x00
    std::uint32_t muSize;  // +0x04
};
// sizeof(DataBuffer) == 8 bytes.

} // namespace RealmcIface
