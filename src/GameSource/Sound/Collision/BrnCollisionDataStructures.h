#ifndef BRN_SOUND_LOGIC_COLLISION_DATA_STRUCTURES_H
#define BRN_SOUND_LOGIC_COLLISION_DATA_STRUCTURES_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::Collision::ScrapeInfo
//   GameSource/Sound/Collision/BrnCollisionDataStructures.h (assert-cited home) +
//   GameSource/Sound/Collision/BrnCollisionDataStructures.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// ScrapeInfo is the per-scrape descriptor the collision sound logic keeps in a
// short history ring: it identifies a scrape by the pair of collision-object
// identifiers it runs between (maObjectId[0..1]) and carries the running scrape
// audio parameters (mfParam0x18, mfParam0x24). The owning state machine compares
// two ScrapeInfos for "same scrape" (operator==, order-insensitive on the object
// pair) and rolls the audio parameters forward (UpdateHistory).
//
// This TU bodies exactly TWO ledger functions (both assert-cited to
// BrnCollisionDataStructures.h):
//   operator==     @ 0x826821F0   (BrnCollisionDataStructures.h:111/112 assert sites)
//   UpdateHistory  @ 0x826822C8   (BrnCollisionDataStructures.h:146 assert site)
//
// LAYOUT (recovered store-for-store from the asm; offsets are this struct's field
// offsets, all reached BY NAME in the .cpp):
//   lwz/cmplw 0x10, 0x14   -> two u32 object identifiers (compared as the scrape
//                             "key"; order-insensitive: {a,b} == {b,a})
//   lfs/stfs  0x18         -> f32 scrape audio parameter (rolled by UpdateHistory)
//   lwz/cmpw  0x20         -> u32 scrape "kind"/material key (must match exactly)
//   lfs/stfs  0x24         -> f32 scrape audio parameter (rolled by UpdateHistory)
//   lbz       0x29         -> u8 mbValid (asserted set in operator==)
//
// FLAG (un-DWARF'd member names/types): no DecFIGS DWARF hint exists for
// BrnCollisionDataStructures.h, so the members are modelled as HONEST named fields
// at the asm-observed offsets/widths. Only the offsets, the store widths
// (lwz=u32, lfs=f32, lbz=u8) and the comparison/copy semantics are X360 facts; the
// field NAMES are descriptive. The gaps (0x00..0x0F, 0x1C, 0x28, 0x2A..) are other
// ScrapeInfo state NOT touched by these two functions and are modelled as honest
// reserved padding so the offsets land exactly.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// BrnCollisionDataStructures.h (assert-cited home). Per-scrape descriptor.
struct ScrapeInfo
{
    // BrnCollisionDataStructures.h:108 (assert-cited region). True iff the two
    // descriptors identify the same scrape: same kind (mu32Kind) and the same
    // unordered pair of object identifiers ({A,B} matches {A,B} or {B,A}). Asserts
    // both operands are valid (h:111/112). @ 0x826821F0.
    bool operator==( const ScrapeInfo& lInfo ) const;

    // BrnCollisionDataStructures.h:146 (assert site). Roll this descriptor's audio
    // parameters forward from a matching descriptor; asserts *this == lInfo first.
    // @ 0x826822C8.
    void UpdateHistory( const ScrapeInfo& lInfo );

    // -- FLAGGED layout (offsets/widths are X360 facts; field names descriptive) --

    u8  maReserved0x00[0x10];   // @0x00..0x0F: scrape state untouched by these fns

    u32 maObjectId[2];          // @0x10 / 0x14: unordered object-pair "key"

    f32 mfParam0x18;            // @0x18: scrape audio parameter (rolled in UpdateHistory)

    u8  maReserved0x1C[4];      // @0x1C..0x1F: untouched

    u32 mu32Kind;               // @0x20: scrape kind / material key (exact match)

    f32 mfParam0x24;            // @0x24: scrape audio parameter (rolled in UpdateHistory)

    u8  maReserved0x28[1];      // @0x28: untouched

    u8  mbValid;                // @0x29: validity flag (asserted set in operator==)
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_DATA_STRUCTURES_H
