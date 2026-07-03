#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"          // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"      // BrnPhysics::Props::UpdatePropEvent (112-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Props::UpdatePropEvent>::AddEvent
//   @ X360 0x825E5DF8 (dossier id "class:BrnPhysics::Props::UpdatePropEvent>", funcs: 2;
//   this TU covers AddEvent only -- the sibling Append @ 0x825E61F0 is a separate instantiation).
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL  (CgsBaseEventQueue.h:312, `lwz r11,0(r30)`; bne skips) -- tripwire only;
//   * miLength < miMaxLength ("Reached Max length", CgsBaseEventQueue.h:313, `lwz r11,8(r30)` vs
//     `lwz r10,4(r30)`, blt skips) -- tripwire only; the copy below always runs;
//   * store at a 112-byte stride: `mulli r11,r10,0x70` (miLength*112), `add r11,r11,r9`
//     (+mpEvents @0(r30)), then six lvx128/stvx128 pairs (offsets 0,16,32,48,64,80 == 96 bytes)
//     plus word@0x60 (PropEntityID), two s16 half-stores @0x64/0x66, and a byte@0x68 (bool) --
//     i.e. sizeof(UpdatePropEvent) == 112, an X360-ATTESTED 0x70 stride;
//   * bumps miLength (`addi r10,r10,1`/`stw r10,8(r30)`); returns 1 (`li r3,1`).
//
// Member offsets read from the asm: mpEvents @+0, miMaxLength @+4, miLength @+8 (consistent with
// the generic BaseEventQueue<T> layout in CgsBaseEventQueue.h).
template bool
CgsModule::BaseEventQueue<BrnPhysics::Props::UpdatePropEvent>::AddEvent(
    const BrnPhysics::Props::UpdatePropEvent&);
