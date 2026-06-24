#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                            // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"        // BrnPhysics::Deformation::DeactivateDeformationModelEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::DeactivateDeformationModelEvent>::AddEvent @ 0x825E55B8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360
// body appends UNCONDITIONALLY (the two asserts are non-gating tripwires):
//   - assert mpEvents != NULL (CgsBaseEventQueue.h:312)
//   - the overflow tripwire (miLength >= miMaxLength) builds the StrStream message
//     "CgsModule::BaseEventQueue<class BrnPhysics::Deformation::DeactivateDeformationModelEvent>::AddEvent
//      \nReached Max length <miMaxLength>\n" and fires it (:313) -- non-gating.
//   - copies the element with two 64-bit stores (`std` at +0 and +8 == 16 bytes:
//     `*v=*src; v[1]=src[1]`) to mpEvents + 16*miLength (`slwi r,len,4`), then ++miLength,
//     returns 1.
// The 16-byte stride matches sizeof(DeactivateDeformationModelEvent) == alignas(16){
// RigidBodyId(4) + f32(4) + DeformationResetType(4) } rounded to 16. Callers (X360 xrefs):
// VehicleManager::Process{Remove,Reset,CrashingNetworkCars}Events,
// PhysicalTrafficManager::PhysicallyUncrashTrafficCar.
// X360-attested element stride (`slwi r,len,4` == *16; two 64-bit `std` copies).
static_assert(sizeof(BrnPhysics::Deformation::DeactivateDeformationModelEvent) == 16,
              "DeactivateDeformationModelEvent stride 16");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::DeactivateDeformationModelEvent>::AddEvent(
    const BrnPhysics::Deformation::DeactivateDeformationModelEvent&);
