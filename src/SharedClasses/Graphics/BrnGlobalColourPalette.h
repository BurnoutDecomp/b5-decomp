#ifndef BRN_GLOBAL_COLOUR_PALETTE_H
#define BRN_GLOBAL_COLOUR_PALETTE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (== rw::math::vpu::Vector4)

// ============================================================================
// SharedClasses/Graphics/BrnGlobalColourPalette.h
//
// The serialised global car-colour palette resource (CgsResource type 0x1001E /
// 65566). This is the resource CgsResource::PlayerCarColoursResourceType
// (FixUp/FixDown/Serialise @ 0x8267E0xx) loads and rebases, and that a
// CgsResource::ResourcePtr<...> wraps. It is the SECOND resource inside
// Vehicles/VehicleList.bundle.
//
// LAYOUT -- 12 bytes per palette entry, with THIRTY-TWO-BIT pointer columns:
//   BrnWorld::PlayerCarColourPalette  (12 bytes)
//     +0  u32 muPaintColours   // serialised 32-bit slot, FixUp-rebased -> Vector4*
//     +4  u32 muPearlColours   // serialised 32-bit slot, FixUp-rebased -> Vector4*
//     +8  s32 miNumColours
//   BrnWorld::GlobalColourPalette   (48 bytes)
//     +0  PlayerCarColourPalette maPalettes[4]   // eNumPalettes == 4
//
// ⭐ THE 12-BYTE / 32-BIT-SLOT FORM IS PROVEN TWICE, NOT ASSUMED (2026-08-02).
// This header used to declare the two columns as host `Vector4*`, which makes the
// record 24 bytes on x64 -- the exact "serialised slots stay 32-bit" trap. Both
// proofs are reproducible:
//
//   (1) THE SHIPPED PLATFORM-4 BYTES. build/game/Vehicles/VehicleList.bundle
//       resource[1] (id 0x8673D3A8, type 0x1001E) is 2512 bytes. Read as 12-byte
//       records it is exactly self-consistent -- four palettes of {25, 25, 25, 2}
//       colours whose eight colour-array offsets chain end to end with no gaps
//       (0x30 -> 0x1C0 -> 0x350 -> 0x4E0 -> 0x670 -> 0x800 -> 0x990 -> 0x9B0 ->
//       0x9D0 == 2512), and GetSerialisedResourceDescriptor's own size formula
//       `16 * word[0x2C] + word[0x28] - base` == 16*2 + 0x9B0 == 2512 reproduces
//       the payload size to the byte. Read as 24-byte records every field is
//       nonsense (entry 0 would be paint=0x1C000000030, num=1248).
//
//   (2) THE CONSOLE'S OWN INDEX ARITHMETIC. BrnGui::WorldDataController::
//       GetColourPaletteFromType @0x824BDA40 ends
//           slwi r11, r31, 1 ; add r11, r31, r11 ; slwi r11, r11, 2 ; add r3, r11, r30
//       i.e. `base + ((type + type*2) * 4)` == `base + 12*type`. Stride 12.
//
// The two columns hold RESOURCE-RELATIVE OFFSETS on disk and absolute addresses
// after PlayerCarColoursResourceType::FixUp has added the load base -- the same
// contract as VehicleListResource::muEntriesOffset, and safe for the same measured
// reason: the GameData resource roots are carved below 4 GB (BrnGame.log line 3,
// "GameDataRoot0=0x0000000000420000 GameDataRoot1=0x000000000A430000 (below 4GB:
// OK for PointerFromU32)"). Widening either column would desynchronise the struct
// from the shipped bytes and make maPalettes[] walk the wrong stride.
//
// FLAG: the member NAMES and the 4-entry count come from the
// PlayerCarColoursResourceType reconstruction (its 12-byte stride x 4) and the
// DWARF's PlayerCarColours.h enum; there is no DWARF for this record itself.
// ============================================================================

namespace BrnWorld
{
// One car-colour palette entry: paint + pearl colour arrays and the entry count.
// Serialised record -- POD, 12 bytes, no host pointers. Read the two colour arrays
// through GetPaintColours()/GetPearlColours() (post-FixUp); miNumColours is direct.
struct PlayerCarColourPalette
{
    u32 muPaintColours;   // +0  serialised 32-bit column (FixUp-rebased)
    u32 muPearlColours;   // +4  serialised 32-bit column (FixUp-rebased)
    s32 miNumColours;     // +8

    // The X360 walks both columns by 16 bytes (one Vector4) per entry:
    // `*&v22[i] = *palette + 16*i` / `*&v23[i] = palette[1] + 16*i`.
    const Vector4* GetPaintColours() const
    {
        return reinterpret_cast<const Vector4*>(static_cast<uintptr_t>(muPaintColours));
    }

    const Vector4* GetPearlColours() const
    {
        return reinterpret_cast<const Vector4*>(static_cast<uintptr_t>(muPearlColours));
    }

    s32 GetNumColours() const { return miNumColours; }
};

// eNumPalettes == 4 (the PlayerCarColoursResourceType rebase loop count).
enum { E_NUM_PALETTES = 4 };

// The global colour palette resource: four PlayerCarColourPalette entries.
struct GlobalColourPalette
{
    PlayerCarColourPalette maPalettes[E_NUM_PALETTES];   // +0
};

// The serialised record sizes. These are NOT console byte-size literals carried over
// blindly -- both are re-derived above from the shipped platform-4 payload AND from
// the console's own 12-byte index arithmetic, so pinning them is what keeps a future
// widening from silently reintroducing the wrong stride.
static_assert(sizeof(PlayerCarColourPalette) == 12,
              "PlayerCarColourPalette is the 12-byte SERIALISED record (32-bit colour-array columns)");
static_assert(sizeof(GlobalColourPalette) == 48,
              "GlobalColourPalette is 4 x 12 bytes");
}

#endif // BRN_GLOBAL_COLOUR_PALETTE_H
