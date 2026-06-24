#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // BrnPhysics::Deformation::AddDeformationModelEvent (160-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::AddDeformationModelEvent>::AddEvent @ 0x825E52B0
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The generic
// BaseEventQueue<T>::AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin
// explicit instantiation. The X360 body appends UNCONDITIONALLY (the two asserts are non-gating
// tripwires, not a bounds gate):
//   - assert mpEvents != NULL (CgsBaseEventQueue.h:312, `lwz r11,0(this)`; bne skips the assert);
//   - the overflow tripwire (miLength >= miMaxLength: `lwz r11,8(this)` (miLength) vs
//     `lwz r10,4(this)` (miMaxLength), `blt` skips) builds the StrStream message
//     "CgsModule::BaseEventQueue<class BrnPhysics::Deformation::AddDeformationModelEvent>::AddEvent
//      \nReached Max length <miMaxLength>\n" and fires it (:313) -- non-gating; the copy below runs
//     regardless.
//   - copies the 160-byte element to mpEvents[miLength] at a 160-byte stride
//     (`slwi r7,r11,2` == miLength*4; `add r11,r11,r7` == miLength*5; `slwi r11,r11,5` == *32 ==
//     miLength*160; `add r11,r11,r9` == mpEvents + miLength*160), then bumps miLength
//     (`stw r28,8(this)` with r28 == miLength+1) and returns 1.
// The X360 element copy is a head of scalar stores (stw +0, stw +4, std +8, stw +0x10), a body of
// seven 16-byte VMX lanes (stvx128 at +0x20/+0x30/+0x40/+0x50/+0x60/+0x70/+0x80) and a tail
// (stw +0x90, stfs +0x94, stw +0x98, stb +0x9C) -- i.e. it copies the whole 160-byte element image
// (the +0x14..0x1F gap is dead inter-member padding the asm skips). Modelled here as the generic
// `mpEvents[miLength] = lEvent` whole-struct copy-assignment, which copies every member of the
// 160-byte element.
//
// X360-attested element stride (miLength*160; sizeof(AddDeformationModelEvent) == 0xA0).
// Callers (X360 xrefs): Deformation::DeformationInputInterface::AddDeformationModel,
// Vehicle::PhysicalTrafficManager::SendCreateRemoveTrafficEvents.
static_assert(sizeof(BrnPhysics::Deformation::AddDeformationModelEvent) == 160,
              "AddDeformationModelEvent stride 160");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::AddDeformationModelEvent>::AddEvent(
    const BrnPhysics::Deformation::AddDeformationModelEvent&);
