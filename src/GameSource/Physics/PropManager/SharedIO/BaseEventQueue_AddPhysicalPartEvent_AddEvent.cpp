#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"                  // BrnPhysics::Props::AddPhysicalPartEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Props::AddPhysicalPartEvent>::AddEvent  @ X360 0x822C8498
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. Called by BrnPhysics::Props::PropInputInterface::AddPartInstance. The X360 body
// matches the generic store-for-store: appends at an 80-byte stride (r11 = miLength*5 via
// slwi r9,r11,2; add r11,r11,r9 then <<4 == miLength*80, added to *mpEvents), copies the element
// body as four 16-byte VMX block stores (Matrix44Affine mTransform, 64 bytes @0..0x3F) then the
// scalar tail: stw @0x40 (PropEntityID mEntityId), sth @0x44 (miPropTypeId), sth @0x46 (miPartId),
// sth @0x48 (miSlot); bumps miLength. sizeof(AddPhysicalPartEvent) == Matrix44Affine(64) +
// PropEntityID(4) + 3*i16(6) rounded up to the 16-byte alignment (leading Matrix44Affine) == 80,
// the stride the queue is faithful to. The two asserts (mpEvents != NULL @ CgsBaseEventQueue.h:312
// and the max-length tripwire @:313) are the generic body's non-gating tripwires.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Props::AddPhysicalPartEvent>::AddEvent(
    const BrnPhysics::Props::AddPhysicalPartEvent&);
