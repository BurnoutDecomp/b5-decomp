#pragma once

// ===========================================================================
// RealmcIface::CardData -- a Realmc memory-card "card data" record used by the
// X360 memory-card interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX). This header is the canonical OWNING home for the
// CardData struct and its six member functions:
//
//     RealmcIface::CardData::CardData       @ 0x82B53188
//     RealmcIface::CardData::operator=      @ 0x82B531E8
//     RealmcIface::CardData::Empty          @ 0x82B53250
//     RealmcIface::CardData::PreviousState  @ 0x82B532C8
//     RealmcIface::CardData::operator==     @ 0x82B53318
//     RealmcIface::CardData::operator!=     @ 0x82B53330
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, CardData) are preserved
// verbatim per the naming convention. Sibling to RealmcCore.h (wave-4).
//
// LAYOUT (from asm -- the ctor memsets two adjacent 0x50 (80-byte) blocks and
// operator= shuffles them):
//
//   +0x00  maCurrent  -- the active card-data block, 80 bytes.
//   +0x50  maPrevious -- the prior card-data block, 80 bytes.
//                        Total object size = 0xA0 (160) bytes.
//
//   The first 32-bit word of each block (offset +0 within the block) is a
//   state/identifier word: operator== / operator!= compare ONLY that leading
//   word of each object (*a1 vs *a2), and operator= snapshots the current block
//   into the previous block whenever that leading word changes. The remaining
//   76 bytes of each block are opaque card payload (device/content id, name,
//   etc.) memcpy'd wholesale by the X360, so they are modelled as a raw byte
//   tail rather than fabricated named members.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * Empty() returns a shared, lazily-initialised singleton CardData living in
//     Realmc static storage (X360: dword_832500F0 = the object, dword_83250190
//     = a one-shot "initialised" flag word, bit 0). Modelled here with a
//     function-local guard + static instance; the X360 used a raw global flag
//     word, but the observable behaviour (zero-init once, then return the same
//     object) is identical.
// ===========================================================================

#include <cstdint>

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// CardData::Block -- one 80-byte card-data record. The leading word is the
// state/id compared by the relational operators; the tail is opaque payload.
// ---------------------------------------------------------------------------
struct CardDataBlock
{
    std::uint32_t muState;       // +0x00  state/id word (the compare key)
    std::uint8_t  maPayload[76]; // +0x04  opaque card payload (76 bytes)
};
// 0x04 + 76 == 0x50 (80) bytes per block.

class CardData
{
public:
    // @ 0x82B53188 -- zero both blocks (memset 80 + memset 80; leading words
    //                 explicitly re-zeroed).
    CardData();

    // @ 0x82B531E8 -- assign: if the incoming state word differs from ours,
    //                 first snapshot the current block into the previous block,
    //                 then copy the source's current block over ours.
    CardData& operator=(const CardData& rOther);

    // @ 0x82B53318 -- equal iff the leading state words match.
    bool operator==(const CardData& rOther) const;

    // @ 0x82B53330 -- inequality (leading state words differ).
    bool operator!=(const CardData& rOther) const;

    // @ 0x82B532C8 -- copy rSource into *this, then overwrite our current block
    //                 with rSource's previous block (i.e. roll *this back to the
    //                 source's previous state).
    CardData& PreviousState(const CardData& rSource);

    // @ 0x82B53250 -- the shared lazily-zeroed empty/sentinel CardData.
    static CardData& Empty();

    CardDataBlock maCurrent;  // +0x00
    CardDataBlock maPrevious; // +0x50
};
// sizeof(CardData) == 0xA0 (160) bytes.

} // namespace RealmcIface
