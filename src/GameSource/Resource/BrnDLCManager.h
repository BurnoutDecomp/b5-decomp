#ifndef GAMESOURCE_RESOURCE_BRNDLCMANAGER_H
#define GAMESOURCE_RESOURCE_BRNDLCMANAGER_H

#include "types.hpp"

// ============================================================================
// GameSource/Resource/BrnDLCManager.h
//
// MINIMAL SLICE -- models only the downloadable-content availability flags that
// the boot-legal flow touches. The full DLC manager (BrnResource::DLCManager and
// its package list, the DLC debug component, and the feature-availability table)
// lands when those TUs are reconstructed; GROW this header then, do NOT fork the
// DLC types elsewhere.
//
// Shape recovered from the X360 asm of DLCBeatTheTeamGame::SetEnabledState
// (@0x82472CA8) and its baked assert string at BrnDLCManager.h:405
// ("!(mbIsEnabled && !mbIsAvailable)"): each downloadable game is a two-flag
// record -- availability (whether the content is installed/owned) and enabled
// (whether the gameplay path that uses it is switched on). The invariant the
// assert enforces is that a game cannot be enabled while it is unavailable.
// ============================================================================

namespace BrnResource
{

// ---------------------------------------------------------------------------
// Downloadable-content data packs.
//
// GetPackMask / SetPackAvailabilityState bound-check the pack index against
// these two limits (X360 asm: cmpwi lPack,0 / cmpwi lPack,5 with the baked
// assert strings "lPack >= E_DLC_DATA_PACK_START" and
// "lPack < E_DLC_DATA_PACK_COUNT"). START is the inclusive lower bound (0) and
// COUNT is the exclusive upper bound (5).
// ---------------------------------------------------------------------------
enum E_DLC_DATA_PACK
{
    E_DLC_DATA_PACK_START = 0,
    E_DLC_DATA_PACK_COUNT = 5
};

// Number of feature slots in the availability table. The X360 Construct
// (@0x82662C48) memsets a 100-byte (25-dword) block at +0x1C and writes a
// parallel 25-byte bool block at +0x80; both run 0..24.
enum { KU_DLC_FEATURE_COUNT = 25 };

// ---------------------------------------------------------------------------
// DLCFeatureAvailability
//
// Availability table for downloadable content. Holds, per data pack, the bit
// the pack contributes to the running availability mask, and, per feature, the
// pack it requires plus a default-enabled flag.
//
// Layout recovered store-for-store from BrnResource::DLCFeatureAvailability::
// Construct (@0x82662C48), GetPackMask (@0x82661628) and
// SetPackAvailabilityState (@0x826616A0). Total size 0x99 (153) bytes.
//   +0x00 mbConstructed      byte, set 1 by Construct
//   +0x01 mbField01          byte, set 0 by Construct
//   +0x04 muAvailabilityMask dword, the running mask SetPackAvailabilityState mutates
//   +0x08 mauPackMask[5]     dword per pack {1,2,4,8,16}; GetPackMask returns mauPackMask[lPack]
//   +0x1C maePackForFeature[25] dword per feature (which pack it requires)
//   +0x80 mabFeatureEnabled[25] byte per feature (default-enabled flag)
// ---------------------------------------------------------------------------
class DLCFeatureAvailability
{
public:
    // X360 0x82662C48. Zeroes the mask + table, then writes the default per-pack
    // masks, per-feature pack indices and per-feature enabled flags. (The asm's
    // int return is just memset's r3 leaking through Hex-Rays; the method is void.)
    void Construct();

    // X360 0x82661628. Returns the bit mask the given data pack contributes to the
    // availability mask. Asserts lPack is in [E_DLC_DATA_PACK_START, E_DLC_DATA_PACK_COUNT).
    u32 GetPackMask(s32 liPack) const;

    // X360 0x826616A0. Sets or clears the given pack's bit in the availability mask.
    // When lbAvailable is true the pack's bit is OR'd in; otherwise the mask is AND'd
    // with the pack's bit cleared. Asserts the same pack-index bounds.
    void SetPackAvailabilityState(s32 liPack, bool lbAvailable);

private:
    // Never called; defined in BrnDLCManager.cpp to pin member offsets via offsetof.
    static void _AssertLayout();

    bool mbConstructed;                          // +0x00
    bool mbField01;                              // +0x01
    u8   mPad02[2];                              // +0x02 (alignment to the +0x04 dword)
    u32  muAvailabilityMask;                     // +0x04
    u32  mauPackMask[E_DLC_DATA_PACK_COUNT];     // +0x08 .. +0x18
    s32  maePackForFeature[KU_DLC_FEATURE_COUNT];// +0x1C .. +0x7C
    bool mabFeatureEnabled[KU_DLC_FEATURE_COUNT];// +0x80 .. +0x98
};

// A single downloadable "Beat The Team" online game's availability/enabled state.
// Layout recovered from SetEnabledState: mbIsAvailable @ +0x00, mbIsEnabled @ +0x01.
class DLCBeatTheTeamGame
{
public:
    bool IsAvailable() const { return mbIsAvailable; }
    bool IsEnabled() const   { return mbIsEnabled; }

    // X360 0x82472CA8. Stores the enabled flag, then asserts that the content is not
    // enabled while it is unavailable (BrnDLCManager.h:405). The assert is non-fatal
    // (the binary stores the flag and returns regardless).
    void SetEnabledState(bool lbEnabled);

private:
    bool mbIsAvailable; // +0x00
    bool mbIsEnabled;   // +0x01
};

} // namespace BrnResource

#endif // GAMESOURCE_RESOURCE_BRNDLCMANAGER_H
