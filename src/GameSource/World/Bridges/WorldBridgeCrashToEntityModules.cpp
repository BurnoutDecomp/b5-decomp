#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"  // the real interface type behind both storages

// @ 0x827AAC70 -- append the crash module's staged vehicle-input events into the
// physics module's vehicle-input interface (both getters lock-check their buffer).
// Both null tripwires are NON-gating (the X360 falls through after firing); the
// X360 tail returns the forwarded call's result as a register artifact -- the
// logical return type is void.

namespace WorldModule
{
// @ 0x827AAC70
void BridgeCrashModuleToPhysicsModule(
    void* lpWorldModule,
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutput_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpPhysicsModuleInputBuffer != 0, "lpPhysicsModuleInputBuffer");   // :58
    CGS_ASSERT(lpCrashOutput_PreScene != 0, "lpCrashOutput_PreScene");           // :59

    // FLAG: both buffer slices model this member as opaque storage (their headers'
    // convention); the real type on BOTH sides is BrnPhysics::Vehicle::
    // VehicleInputInterface (the crash buffer stages its own copy of the same
    // interface) -- adopt the typed members when the buffers' own TUs land.
    typedef BrnPhysics::Vehicle::VehicleInputInterface VehicleInputInterface;
    VehicleInputInterface* lpVehicle = reinterpret_cast<VehicleInputInterface*>(
        lpPhysicsModuleInputBuffer->GetVehicleInputInterface());
    lpVehicle->Append(*reinterpret_cast<const VehicleInputInterface*>(
        lpCrashOutput_PreScene->GetVehicleInputInterface()));
}
}
