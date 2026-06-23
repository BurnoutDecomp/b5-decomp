#ifndef BRN_GLOBAL_COLOUR_PALETTE_H
#define BRN_GLOBAL_COLOUR_PALETTE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (== rw::math::vpu::Vector4)

// ============================================================================
// SharedClasses/Graphics/BrnGlobalColourPalette.h
//
// The serialised global car-colour palette resource. This is the resource Type
// that CgsResource::PlayerCarColoursResourceType (FixUp/FixDown/Serialise @
// 0x8267E0xx) loads and rebases, and that a CgsResource::ResourcePtr<...> wraps.
//
// LAYOUT (from the PlayerCarColoursResourceType rebase pass, which walks four
// entries of stride 12 bytes and rebases the two pointer columns at +0/+4):
//   BrnWorld::PlayerCarColourPalette  (12 bytes)
//     +0  Vector4* mpPaintColours    // rebased pointer column
//     +4  Vector4* mpPearlColours    // rebased pointer column
//     +8  s32      miNumColours
//   BrnWorld::GlobalColourPalette
//     +0  PlayerCarColourPalette maPalettes[4]   // KU_NUM_PALETTES == 4
//
// The pointers are 32-bit on the X360 spine (the serialised load-relative form
// the rebase arithmetic adjusts as u32 deltas); modelled as real Vector4*
// pointers here for the host's named-field access (the resource-type pass that
// pokes them as dword columns keeps its own dword view).
//
// FLAG: member names mpPaintColours / mpPearlColours / miNumColours and the
// 4-entry count are taken from the PlayerCarColoursResourceType reconstruction
// (its documented 12-byte stride × 4) -- not from a DWARF for this type, which
// has none. The two pointer columns are the only fields the recovered code
// reads/rebases; miNumColours rounds the stride to 12.
// ============================================================================

namespace BrnWorld
{
// One car-colour palette entry: paint + pearl colour arrays and the entry count.
struct PlayerCarColourPalette
{
    Vector4* mpPaintColours;   // +0  rebased pointer column
    Vector4* mpPearlColours;   // +4  rebased pointer column
    s32      miNumColours;     // +8
};

// eNumPalettes == 4 (the PlayerCarColoursResourceType rebase loop count).
enum { E_NUM_PALETTES = 4 };

// The global colour palette resource: four PlayerCarColourPalette entries.
struct GlobalColourPalette
{
    PlayerCarColourPalette maPalettes[E_NUM_PALETTES];   // +0
};
}

#endif // BRN_GLOBAL_COLOUR_PALETTE_H
