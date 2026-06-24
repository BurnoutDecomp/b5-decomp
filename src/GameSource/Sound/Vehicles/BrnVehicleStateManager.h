#ifndef BRN_SOUND_VEHICLES_VEHICLE_STATE_MANAGER_H
#define BRN_SOUND_VEHICLES_VEHICLE_STATE_MANAGER_H

#include "types.hpp"

// =============================================================================
// BrnSound::Vehicles::VehicleStateManager
//   GameSource/Sound/Vehicles/BrnVehicleStateManager.h (assert-cited home) +
//   GameSource/Sound/Vehicles/BrnVehicleStateManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// VehicleStateManager owns the per-active-race-car sound state. AI cars are
// assigned to a fixed bank of engine-sound "voices"; GetAIEngineAssignment maps an
// active-race-car index (0..7) to its engine-voice slot through a static lookup
// table. The result is consumed by the engine effect attach path
// (BrnSound::Vehicles::Engines::DualGinsuEffect::Attach).
//
// This TU bodies exactly ONE ledger function:
//   GetAIEngineAssignment  @ 0x82682050  (BrnVehicleStateManager.h:143 assert site)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace VehicleStateManager
{

// BrnVehicleStateManager.h (assert-cited region). Number of active race-car slots;
// the asm bounds-checks the index against this (cmpwi r31, 8 ; the assert text is
// "liVehicleIndex >= 0 && liVehicleIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT").
const s32 E_ACTIVE_RACE_CAR_INDEX_COUNT = 8;

// BrnVehicleStateManager.h:143 (assert site). Map an active-race-car index to its
// AI engine-voice assignment via the static lookup table. Asserts the index is in
// [0, E_ACTIVE_RACE_CAR_INDEX_COUNT). Returns the assigned voice (a u8 from the
// table). @ 0x82682050.
u8 GetAIEngineAssignment( u32 luVehicleIndex );

} // namespace VehicleStateManager
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_VEHICLE_STATE_MANAGER_H
