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

    CGS_ASSERT( lpPhysicsModuleInputBuffer != 0, "lpPhysicsModuleInputBuffer" );   // :58
    CGS_ASSERT( lpCrashOutput_PreScene != 0, "lpCrashOutput_PreScene" );           // :59

    const BrnPhysics::Vehicle::VehicleInputInterface* lpCrashVehicleInput =
        lpCrashOutput_PreScene->GetVehicleInputInterface();                        // 0x827AACD8
    lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->Append( *lpCrashVehicleInput );   // 0x827AACE4 / 0x827AACEC

}
}
