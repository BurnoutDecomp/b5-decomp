#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"                  // BrnPhysics::Props::AddPhysicalPropEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Props::AddPhysicalPropEvent>::AddEvent  @ X360 0x822C8308
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. Called by BrnPhysics::Props::PropInputInterface::AddPropInstance. The X360 body
// matches the generic store-for-store: appends at an 80-byte stride (miLength*5 then <<4, added to
// *mpEvents), copies the element body as four 16-byte VMX block stores (Matrix44Affine mTransform,
// 64 bytes @0..0x3F) then the scalar tail: stw @0x40 (PropEntityID mEntityId), stw @0x44
// (EPropState meState, 4B), sth @0x48 (miPropTypeId), sth @0x4A (miSlot), stb @0x4C
// (mbAddExtraComOffset); bumps miLength. sizeof(AddPhysicalPropEvent) == Matrix44Affine(64) +
// PropEntityID(4) + EPropState(4) + 2*i16(4) + bool(1) rounded up to the 16-byte alignment == 80.
// The two asserts (mpEvents != NULL @ CgsBaseEventQueue.h:312 and the max-length tripwire @:313)
// are the generic body's non-gating tripwires.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Props::AddPhysicalPropEvent>::AddEvent(
    const BrnPhysics::Props::AddPhysicalPropEvent&);
