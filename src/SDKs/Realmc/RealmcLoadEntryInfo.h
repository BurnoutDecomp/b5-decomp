#pragma once

// ===========================================================================
// RealmcIface::LoadEntryInfo -- a Realmc save/load "load-entry info" record used
// by the X360 memory-card / save-load interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX; sibling to RealmcIface::CardData in RealmcCardData.h
// and the RealmcCore primitives in RealmcCore.h).
//
// This header is the canonical OWNING home for the LoadEntryInfo struct and its
// two reconstructed member functions:
//
//     RealmcIface::LoadEntryInfo::LoadEntryInfo  @ 0x82B519E8   (default ctor)
//     RealmcIface::LoadEntryInfo::operator=      @ 0x82B51A78   (copy assign)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, LoadEntryInfo) are preserved
// verbatim per the naming convention.
//
// LAYOUT (from asm -- the ctor zero-stores five fields, operator= copies a
// 32-byte head plus four trailing 32-bit words):
//
//   +0x00              a 32-byte head block. operator= memcpy's it wholesale from
//                      the source; the ctor zeroes ONLY its leading byte (+0x00),
//                      and operator= zeroes the LAST byte (+0x1F) after the copy
//                      (a one-byte flag/terminator). The interior 30 bytes are
//                      opaque save-entry payload (file/slot identifiers, name,
//                      etc.) the X360 moves wholesale, so they are modelled as a
//                      raw byte block rather than fabricated named members.
//   +0x20 .. +0x2C     four 32-bit words. The ctor zero-stores each individually;
//                      operator= copies each individually (NOT via the head
//                      memcpy). Opaque to this TU -> modelled as a u32[4] tail.
//
//   Total modelled size = 0x30 (48) bytes.
//
// STORE ORDER (operator=, exact): the four trailing words are copied FIRST
// (+0x20, +0x24, +0x28, +0x2C in that order), THEN the 32-byte head is memcpy'd,
// THEN the +0x1F flag byte is zeroed last. Reproduced verbatim in the .cpp.
// ===========================================================================

#include <cstdint>

namespace RealmcIface
{

class LoadEntryInfo
{
public:
    // @ 0x82B519E8 -- zero the four trailing words and the leading head byte.
    LoadEntryInfo();

    // @ 0x82B51A78 -- copy the four trailing words, then the 32-byte head, then
    //                 clear the +0x1F flag byte.
    LoadEntryInfo& operator=(const LoadEntryInfo& rOther);

    std::uint8_t  maHead[0x20];     // +0x00  opaque head; +0x1F is a flag byte
    std::uint32_t maTrailing[4];    // +0x20  four opaque 32-bit words
};
// sizeof(LoadEntryInfo) == 0x30 (48) bytes.

} // namespace RealmcIface
