#ifndef BRN_SOUND_VEHICLES_PLAYER_VEHICLE_STATE_H
#define BRN_SOUND_VEHICLES_PLAYER_VEHICLE_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Vehicles::PlayerVehicleState
//   GameSource/Sound/Vehicles/BrnPlayerVehicleState.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PlayerVehicleState is the sound-logic state node for the PLAYER car's engine
// audio -- the sibling of AIVehicleState. DWARF's true intermediate parent
// BrnSound::Vehicles::VehicleState has its full body DEFERRED; deriving directly
// from the committed BrnState base keeps the destructor self-contained and is the
// established sibling pattern (BrnCollisionState / BrnGlobalState do the same).
// The remaining DWARF surface (GetTypeInfo/GetTypeName/CreateObject/
// GetStaticTypeInfo, Attach/UpdateParams/Detach, mRaceCarState) is DEFERRED. This
// TU bodies only the destructor (@ 0x826CA250).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the destructor re-installs the two
// vtable pointers @ +0 (compiler-synthesised) and calls the base's
// DestroyEffects(); no member offsets are asserted.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

struct PlayerVehicleState : public BrnSound::Logic::BrnState
{
    PlayerVehicleState() {}

    // @ 0x826CA250 -- the X360 `scalar deleting destructor'. Installs
    // PlayerVehicleState's own vtable, calls State::DestroyEffects(), re-installs the
    // MemBase base vtable, and (deleting flavour) frees the storage through the sound
    // allocator (host `delete` stands in). Bodied out-of-line in
    // BrnPlayerVehicleState.cpp.
    virtual ~PlayerVehicleState();

    // -- per-class RTTI. DEFERRED bodies (declared for the state vtable shape).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_PLAYER_VEHICLE_STATE_H
