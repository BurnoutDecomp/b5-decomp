#pragma once

// ===========================================================================
// RealmcIface::GameInfo -- a Realmc save/load "game info" record used by the
// X360 memory-card / save-load interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX; sibling to RealmcIface::CardData in RealmcCardData.h
// and the RealmcCore primitives in RealmcCore.h).
//
// This header is the canonical OWNING home for the GameInfo struct and its one
// reconstructed member function:
//
//     RealmcIface::GameInfo::GameInfo  @ 0x82B51940   (ctor from a source blob)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, GameInfo) are preserved
// verbatim per the naming convention.
//
// LAYOUT (from asm -- the ctor memcpy's a 66-byte head from the source then
// zero-stores one u16 and three u32 fields):
//
//   +0x00  maName    -- a 64-byte name/identifier buffer copied wholesale from
//                       the source (part of the 0x42-byte memcpy). Modelled as a
//                       raw byte block (opaque content) rather than fabricated
//                       named members.
//   +0x40  muField40 -- a u16 included in the 0x42-byte copy but immediately
//                       re-zeroed (sth r30, 0x40) -- i.e. always 0 after ctor.
//   +0x44  muField44 -- u32, zero-stored.
//   +0x48  muField48 -- u32, zero-stored.
//   +0x4C  muField4C -- u32, zero-stored.
//
// The 0x42 (66) byte memcpy spans +0x00..+0x41 (name + the +0x40 u16); the u16
// is then explicitly zeroed, and the three trailing u32s are zero-initialised.
// ===========================================================================

#include <cstddef>
#include <cstdint>

namespace RealmcIface
{

class GameInfo
{
public:
    // @ 0x82B51940 -- copy a 66-byte head (maName + muField40) from pSource,
    //                 then zero muField40 and the three trailing u32 fields.
    //                 pSource points at a 66-byte source blob.
    explicit GameInfo(const void* pSource);

    std::uint8_t  maName[64];  // +0x00  64-byte name buffer
    std::uint16_t muField40;   // +0x40  re-zeroed after the copy
    std::uint32_t muField44;   // +0x44
    std::uint32_t muField48;   // +0x48
    std::uint32_t muField4C;   // +0x4C
};
// sizeof(GameInfo) == 0x50 (80) bytes.

} // namespace RealmcIface
