#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"                  // BrnPhysics::Props::AddPhysicalPropEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Props::AddPhysicalPropEvent>::Append  @ X360 0x827A7E50
// The generic Append body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. Called by BrnPhysics::Props::PropInputInterface::Append and
// WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics. The X360 body matches the generic
// store-for-store: XMemCpy(pDest = *mpEvents + 80*miLength, pSrc = *src.mpEvents,
// count = 80*src.miLength) -- the *5-then-<<4 index math on both the dest offset and the byte count
// gives an 80-byte stride -- then miLength += src.miLength. Cross-confirmed by the sibling AddEvent
// @0x822C8308. sizeof(AddPhysicalPropEvent) == Matrix44Affine(64) + PropEntityID(4) +
// EPropState(4) + 2*i16(4) + bool(1) rounded up to the 16-byte alignment == 80. The three asserts
// (mpEvents != NULL @:413, 'Base event queue overflow' @:414, source mpEvents != NULL @:486) are
// the generic body's non-gating tripwires.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Props::AddPhysicalPropEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Props::AddPhysicalPropEvent>&);
