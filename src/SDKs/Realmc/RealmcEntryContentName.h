#pragma once

// ===========================================================================
// RealmcIface::EntryContentName -- the "entry content" descriptor the X360
// memory-card / save-load interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX) carries for each save entry: a display title, a
// device file name, and a derived size word. Sibling of RealmcIface::SaveReq
// (RealmcSaveReq.h) and RealmcIface::CardData (RealmcCardData.h).
//
// This header is the canonical OWNING home for the three reconstructed
// EntryContentName member functions:
//
//     RealmcIface::EntryContentName::SetTitle          @ 0x82B51AD8
//     RealmcIface::EntryContentName::SetFileName       @ 0x82B51B28
//     RealmcIface::EntryContentName::EntryContentName  @ 0x82B51C10  (ctor)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, EntryContentName, SetTitle,
// SetFileName) are preserved verbatim per the naming convention.
//
// LAYOUT (from the ctor / SetTitle / SetFileName asm, and the sibling copy
// helpers sub_82B51C70 / sub_82B51D10 which walk the same offsets):
//
//   +0x000  macTitle    -- a 256-byte title buffer. SetTitle memcpy's 256 bytes
//                          from the source (or memset's 256 zero bytes when the
//                          source is null), then zero-stores the 16-bit word at
//                          +0xFE (NUL-terminating the last UTF-16-style slot).
//   +0x100  macFileName -- a 42-byte (0x2A) device file-name buffer. SetFileName
//                          strncpy's at most 42 bytes from the source then
//                          zero-stores the byte at +0x129 (NUL terminator); when
//                          the source is null it just zero-stores +0x100.
//   +0x12C  mnSize      -- a 32-bit integer derived in the ctor from arg a4:
//                          mnSize = (int)((float)a4 * 2.8f) (truncate toward
//                          zero). The X360 sign-extends a4 to 64-bit, converts to
//                          double, narrows to float, multiplies by the rodata
//                          constant flt_8200C6B8 (IDA-decoded 2.8), then fctiwz
//                          (round-toward-zero) + stfiwx stores the integer word.
//   +0x130  muField130  -- a 32-bit word. NOT written by this TU's three
//                          functions; the sibling copy helper (sub_82B51D10)
//                          copies it. Modelled by name as the slot the copy path
//                          touches; left untouched by the ctor (matching the asm).
//   +0x134  muField134  -- a 32-bit word, same provenance as muField130.
//
// sizeof(EntryContentName) == 0x138 (312) bytes -- matching the 0x138-byte
// embedded params block RealmcIface::SaveReq carries at its +0x20 (see
// RealmcSaveReq.h, maParams[0x138]).
// ===========================================================================

#include <cstdint>

namespace RealmcIface
{

class EntryContentName
{
public:
    // @ 0x82B51C10 -- SetTitle(pTitleSource), SetFileName(pFileNameSource), then
    //                 mnSize = (int)((float)nSizeSeed * 2.8f). The IDA signature
    //                 lists (a1,a2,a3,a4); a2 is the title source, a3 the file-name
    //                 source, a4 the size seed.
    EntryContentName(const void* pTitleSource,
                     const char* pFileNameSource,
                     int nSizeSeed);

    // @ 0x82B51AD8 -- copy 256 bytes from pTitleSource into macTitle (or zero the
    //                 256 bytes when pTitleSource is null), then zero the 16-bit
    //                 word at +0xFE. Returns this (the X360 returns r3).
    EntryContentName* SetTitle(const void* pTitleSource);

    // @ 0x82B51B28 -- strncpy at most 42 bytes from pFileNameSource into
    //                 macFileName + zero-store the byte at +0x129 (NUL); when
    //                 pFileNameSource is null, zero-store macFileName[0] only.
    //                 Returns this.
    EntryContentName* SetFileName(const char* pFileNameSource);

private:
    std::uint8_t  macTitle[0x100];    // +0x000  256-byte title (word @ +0xFE zeroed)
    char          macFileName[0x2A];  // +0x100  42-byte file name (NUL @ +0x129)
    std::uint8_t  maPad12A[2];        // +0x12A  alignment padding before mnSize
    std::int32_t  mnSize;             // +0x12C  (int)((float)nSizeSeed * 2.8f)
    std::uint32_t muField130;         // +0x130  copied by sibling helper, not here
    std::uint32_t muField134;         // +0x134  copied by sibling helper, not here
};
// sizeof(EntryContentName) == 0x138 (312) bytes.

} // namespace RealmcIface
