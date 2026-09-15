#ifndef BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_H
#define BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"

// =============================================================================
// BrnSound::Vehicles::AIVehicleState
//   GameSource/Sound/Vehicles/BrnAIVehicleState.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The sound-logic state node for ONE AI car's engine audio -- the sibling of
// PlayerVehicleState. DWARF (BrnAIVehicleState.h:35):
//   struct AIVehicleState : public BrnSound::Vehicles::VehicleState
// with no members of its own; CreateObject @0x826E2E90 (an export-set hole, read
// with ppcdis) allocates 0x530 bytes, runs VehicleState::VehicleState @0x826C9E70
// and installs the vtable 0x820B3A3C:
//   +0x0C Attach @0x826CA998, +0x10 UpdateParams @0x826EFD18, +0x18 Detach
//   @0x826CACB8, +0x20 VehicleState::IsAttachedToThis @0x82683D38 (inherited).
// RTTI: descriptor 0x82F2E89C {0x20000, "AIVehicleState", base BrnState
// 0x82F2E7DC, &CreateObject}, registered from the CRT init bank @0x82C61DA8.
//
// What the state does that the player state does not: it resolves BOTH engine
// components (engine + exhaust) to the same authored AI engine picked by
// VehicleStateManager::GetAIEngineAssignment(index), and it drops itself as soon
// as its race car stops being active or another state is already detaching.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

struct AIVehicleState : public BrnSound::Vehicles::VehicleState
{
    AIVehicleState() {}
    virtual ~AIVehicleState();                       // @ 0x826CA8F8 (scalar deleting destructor)

    // -- per-class RTTI (DWARF h:37 / cpp:27).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;         // @ 0x826840C8
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();
    static CgsSound::Logic::State* CreateObject( u32 auType );   // @ 0x826E2E90

    virtual void Attach( void* apvAttachment );      // @ 0x826CA998 (DWARF cpp:47)
    virtual void UpdateParams( f32 af32DeltaTime );  // @ 0x826EFD18 (DWARF cpp:106)
    virtual bool Detach();                           // @ 0x826CACB8 (DWARF cpp:137)
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_H
