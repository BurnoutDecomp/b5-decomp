#ifndef GAMESOURCE_RESOURCE_SHAREDIO_BRNASSETIDS_H
#define GAMESOURCE_RESOURCE_SHAREDIO_BRNASSETIDS_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (typedef u64)

// ============================================================================
// GameSource/Resource/SharedIO/BrnAssetIds.h
//
// BrnResource asset-id factory free functions. These build a packed CgsID
// (base-40, see CgsID.h) for a named runtime asset so the streaming system can
// key a resource by id rather than by raw string.
//
// Recovered from the X360 ARTIST build (the baked assert file paths name this
// header verbatim, e.g.
//   "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\resource\\sharedio\\../BrnAssetIds.h").
// The Hex-Rays prototypes carry extra trailing int/__int64 parameters; the asm
// only ever consumes the leading name/index argument, so those are IDA-invented
// garbage (the call sites pass a single argument) and are dropped here.
//
//   MakeVehicleId        @ 0x8229FB50  -> CgsIDCompress("VEH_<name>")
//   MakeTrafficVehicleId @ 0x8274F130  -> CgsIDCompress("TVEH<name>")
//   MakeTrackUnitId      @ 0x8229FC30  -> packed id for a numbered track unit
// ============================================================================

namespace BrnResource
{

// ADDITIVE GROW: the asset-set selector. DWARF home BrnAssetIds.h:33 (this header).
// A resource request names which slice(s) of an asset to act on. Consumed as a 4-byte
// enum field (meType) by the GameData IO event payloads. Values from the DecFIGS DWARF.
enum EAssetSet
{
    E_ASSETSET_GRAPHICS    = 0,
    E_ASSETSET_PHYSICS     = 1,
    E_ASSETSET_SOUND       = 2,
    E_ASSETSET_DATA        = 3,
    E_ASSETSET_ATTRIBS     = 4,
    E_ASSETSET_ALL         = 5,
    E_ASSETSET_FORCE_DWORD = 0xFFFFFFFF
};

// Build the CgsID for a player/race vehicle from its <=8-char asset name. X360
// asserts the name is at most 8 characters ("Vehicle name too long"), formats
// "VEH_<name>" into a 13-byte buffer (KI_CGSID_STRING_LEN) and compresses it.
CgsID MakeVehicleId(const char* lpcName);

// Build the CgsID for a traffic vehicle from its <=8-char asset name. As
// MakeVehicleId but with the "TVEH" prefix; on the over-length assert the X360
// also streams the offending name (or "<NULLSTRING>").
CgsID MakeTrafficVehicleId(const char* lpcName);

// Build the packed CgsID for the numbered track unit luUnitIndex (0..999). X360
// asserts 0 <= luUnitIndex <= 999, then folds the (zero-padded, base-40) decimal
// digits of the index into the fixed unit-name base id. Reconstructed as the exact
// integer fold the X360 emits (see BrnAssetIds.cpp).
CgsID MakeTrackUnitId(u32 luUnitIndex);

} // namespace BrnResource

#endif // GAMESOURCE_RESOURCE_SHAREDIO_BRNASSETIDS_H
