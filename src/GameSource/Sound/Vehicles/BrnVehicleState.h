#ifndef BRN_SOUND_VEHICLES_VEHICLE_STATE_H
#define BRN_SOUND_VEHICLES_VEHICLE_STATE_H

#include "types.hpp"

// =============================================================================
// BrnSound::Vehicles::VehicleState::AttachInfo
//   GameSource/Sound/Vehicles/BrnVehicleState.h (assert-cited home) +
//   GameSource/Sound/Vehicles/BrnVehicleState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// AttachInfo is the small record VehicleState hands the engine-sound attach path:
// it pairs a vehicle asset with the active-race-car index it is being attached to,
// plus a 64-bit attach token. Construct validates the index and asset, then fills
// the three fields.
//
// This TU bodies exactly ONE ledger function:
//   AttachInfo::Construct  @ 0x82681FC8  (BrnVehicleState.h:275/276 assert sites)
//
// STORE MAP (from @0x82681FC8, store widths authoritative):
//   stw  r28, 0x00  -> mpVehicleAsset   (lpVehicleAsset; asserted non-null @:276)
//   stw  r30, 0x04  -> muVehicleIndex   (liVehicleIndex; asserted [0,8) @:275)
//   std  r27, 0x08  -> mAttachToken     (a 64-bit word; std = full 8 bytes)
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): mpVehicleAsset is a 4-byte pointer on
// X360. Members are pinned BY NAME + ORDER mirroring the X360 store sequence; the
// absolute X360 offsets are NOT asserted across the 32/64 pointer-width boundary.
// The 64-bit attach word is modelled as `u64` (the std store width is authoritative;
// the value is sourced from a 32-bit register argument zero/sign-extended into the
// 64-bit GPR on X360, so its high half is the register's extension).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace VehicleState
{

// BrnVehicleState.h (assert-cited region). Upper bound for the active-race-car
// index the attach path validates against (cmpwi r30, 8). The assert text is
// "liVehicleIndex >= 0 && liVehicleIndex < static_cast<int32_t>(BrnWorld::KI_MAX_ACTIVE_RACE_CARS)".
const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

// BrnVehicleState.h:275/276 (assert sites). The per-attach record: which vehicle
// asset is being attached, to which active-race-car index, with a 64-bit token.
struct AttachInfo
{
    // BY NAME. The vehicle asset being attached (asserted non-null @:276). 4-byte
    // pointer on X360; widened on host, offset not asserted across the boundary.
    void* mpVehicleAsset;

    // The active-race-car index (asserted [0, KI_MAX_ACTIVE_RACE_CARS) @:275).
    u32 muVehicleIndex;

    // The 64-bit attach token (std-store; full 8 bytes on X360).
    u64 mAttachToken;

    // @ 0x82681FC8 — validate the index and asset, then store all three fields.
    // Signature mirrors the X360 fastcall register order: r4=token, r5=asset,
    // r6=vehicleIndex. Returns `this`.
    AttachInfo* Construct( u64 aAttachToken, void* apVehicleAsset, u32 auVehicleIndex );
};

} // namespace VehicleState
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_VEHICLE_STATE_H
