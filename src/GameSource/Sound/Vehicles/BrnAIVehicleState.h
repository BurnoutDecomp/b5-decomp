#ifndef BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_H
#define BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Vehicles::AIVehicleState
//   GameSource/Sound/Vehicles/BrnAIVehicleState.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// AIVehicleState is the sound-logic state node for an AI car's engine audio.
// DWARF's true intermediate parent BrnSound::Vehicles::VehicleState (a
// VehicleState : public BrnState) has its full body DEFERRED; deriving directly
// from the committed BrnState base keeps the destructor self-contained and is the
// established sibling pattern (BrnCollisionState / BrnGlobalState do the same).
// The DWARF-attested VehicleState surface (mRaceCarState, Attach/Detach/
// UpdateParams/CreateObject/GetStaticTypeInfo) is DEFERRED (out of this
// destructor's scope). This TU bodies only the destructor (@ 0x826CA8F8).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the destructor touches no member
// offsets (it re-installs the two vtable pointers @ +0 -- compiler-synthesised --
// and calls the base's DestroyEffects()), so none are asserted here.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// DWARF-confirmed AIVehicleState (via VehicleState : public BrnState). Same vtable
// pair (own-vtable install / MemBase base re-install) as the sibling CollisionState
// @ 0x826D3380. Only the surface needed for the destructor's vtable slot is
// declared; the remaining RTTI hook bodies are DEFERRED.
struct AIVehicleState : public BrnSound::Logic::BrnState
{
    AIVehicleState() {}

    // @ 0x826CA8F8 -- the X360 `scalar deleting destructor'. Installs AIVehicleState's
    // own vtable, calls State::DestroyEffects() to tear down attached effects,
    // re-installs the MemBase base vtable as the chain unwinds, and (deleting flavour)
    // routes the storage back through the sound allocator. The observable source-level
    // body is the DestroyEffects() call; the vtable re-installs and allocator-routed
    // free are the compiler-synthesised deleting-destructor parts (host `delete` stands
    // in). Bodied out-of-line in BrnAIVehicleState.cpp.
    virtual ~AIVehicleState();

    // -- per-class RTTI. DEFERRED bodies (declared for the state vtable shape).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_H
