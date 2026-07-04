#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // BrnPhysics::Deformation::SetModelCollisionEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::SetModelCollisionEvent>::AddEvent @ 0x825E5700
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360
// body appends UNCONDITIONALLY (the two asserts are non-gating tripwires):
//   - assert mpEvents != NULL (CgsBaseEventQueue.h:312, `lwz r11,0(this)`; bne skips the assert);
//   - the overflow tripwire (miLength >= miMaxLength: `lwz r11,8(this)` (miLength) vs
//     `lwz r10,4(this)` (miMaxLength), `blt` skips) builds the StrStream message
//     "CgsModule::BaseEventQueue<class BrnPhysics::Deformation::SetModelCollisionEvent>::AddEvent
//      \nReached Max length <miMaxLength>\n" and fires it (:313) -- non-gating; the copy below
//     runs regardless.
//   - copies the element with two 64-bit stores (`std` at +0 and +8 == 16 bytes:
//     `*v=*src; v[1]=src[1]`) to mpEvents + 16*miLength (`slwi r11,miLength,4`), then
//     ++miLength (`stw`,8(this)), returns 1.
// The 16-byte stride matches sizeof(SetModelCollisionEvent) == alignas(16){ RigidBodyId(4) +
// bool(1) } rounded to 16. Callers (X360 xrefs): VehicleManager::ProcessCollisionEvents.
// X360-attested element stride (`slwi r,len,4` == *16; two 64-bit `std` copies).
static_assert(sizeof(BrnPhysics::Deformation::SetModelCollisionEvent) == 16,
              "SetModelCollisionEvent stride 16");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::SetModelCollisionEvent>::AddEvent(
    const BrnPhysics::Deformation::SetModelCollisionEvent&);
