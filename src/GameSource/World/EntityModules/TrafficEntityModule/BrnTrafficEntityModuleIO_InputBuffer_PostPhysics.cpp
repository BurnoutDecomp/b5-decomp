#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics setters, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Each tests the IOBuffer write-lock bit (status>>3 &1 ==
// IsBufferLockedForWriting()), fires the non-gating "Not locked for writing" assert (the corpus
// convention drops the rodata's trailing \n, matching the sibling RaceCarEntityModuleIO /
// BrnWorldModuleIO_UpdateOutputBuffer bodies), then copies the source into the matching embedded
// member via that member's operator= -- the reverse of the X360 XMemCpy the physics/race-car
// bridges emit. Producers: WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics and
// BridgeRaceCarModuleToTrafficModule_PreScene (active-race-car member).

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // X360 0x827A9F50 (:347): write-lock; VehicleOutputInterface::operator= on this+0x10.
    void InputBuffer_PostPhysics::SetVehicleOutputInterface(const VehicleOutputInterface* lpVehicleOutputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mVehicleOutputInterface = *lpVehicleOutputInterface;
    }

    // X360 0x827AA000 (:353): write-lock; VehicleManagerOutputInterface::operator= on this+0xEC30.
    void InputBuffer_PostPhysics::SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface* lpVehicleManagerOutputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mVehicleManagerOutputInterface = *lpVehicleManagerOutputInterface;
    }

    // X360 0x827A06C0 (:359): write-lock; XMemCpy(this+0x128C0, src, 0x28F0 == 10480) reversed
    // into RCEntityActiveRaceCarOutputInterface::operator=.
    void InputBuffer_PostPhysics::SetActiveRaceCarOutputInterface(const ActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mActiveRaceCarOutputInterface = *lpActiveRaceCarOutputInterface;
    }

    // X360 0x827AA0B8 (:362): write-lock; DeformationOutputInterfaceForEntityModules::operator= on this+0x151B0.
    void InputBuffer_PostPhysics::SetDeformationOutputInterfaceForEntityModules(const DeformationOutputInterfaceForEntityModules* lpDeformationOutputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mDeformationOutputInterfaceForEntityModules = *lpDeformationOutputInterface;
    }

    // X360 0x827A0778 (:366): write-lock; single-word store of the source into mContactSpyInterface
    // (this+0x19EF0). ContactSpyInterface is a 4-byte type, so the store is the whole memberwise copy.
    void InputBuffer_PostPhysics::SetContactSpyInterface(const ContactSpyInterface* lpContactSpyInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mContactSpyInterface = *lpContactSpyInterface;
    }
}
}
